"""生成自有glTF示例与回归输入，无外部模型许可或下载依赖。只运行时才写生成资源。"""
import base64
import copy
import json
import math
import pathlib
import struct
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'examples/gltf_model/assets'
TEST = ROOT / 'tests/fixtures/gltf'


def png(gray=False):
    # 四角不同颜色，用于发现UV上下颠倒。无需Pillow等额外依赖。
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    pixels = bytes([0, 255, 0, 0, 255, 0, 255, 0, 255,
                    0, 0, 0, 255, 255, 255, 255, 0, 255])
    if gray:
        pixels = bytes([0,128,128,128,255,128,128,128,255] * 2)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b''))


def document(vertices, indices, index_type=5123):
    # 顶点交错布局：position(12字节)+normal(12字节)+uv(8字节)，验证byteStride。
    vertex_bytes = b''.join(struct.pack('<8f', *v) for v in vertices)
    index_bytes = struct.pack('<' + {5121: 'B', 5123: 'H', 5125: 'I'}[index_type] * len(indices), *indices)
    blob = vertex_bytes + index_bytes
    image = png()
    result = {
        'asset': {'version': '2.0', 'generator': 'Infinite self-authored fixture'},
        'scene': 0, 'scenes': [{'nodes': [0]}],
        'nodes': [{'name': 'assembly', 'children': [1]}, {'name': 'surface', 'mesh': 0}],
        'buffers': [{'byteLength': len(blob), 'uri': 'mesh.bin'}],
        'bufferViews': [{'buffer': 0, 'byteOffset': 0, 'byteLength': len(vertex_bytes), 'byteStride': 32},
                        {'buffer': 0, 'byteOffset': len(vertex_bytes), 'byteLength': len(index_bytes)}],
        'accessors': [
            {'bufferView': 0, 'byteOffset': 0, 'componentType': 5126, 'count': len(vertices), 'type': 'VEC3'},
            {'bufferView': 0, 'byteOffset': 12, 'componentType': 5126, 'count': len(vertices), 'type': 'VEC3'},
            {'bufferView': 0, 'byteOffset': 24, 'componentType': 5126, 'count': len(vertices), 'type': 'VEC2'},
            {'bufferView': 1, 'componentType': index_type, 'count': len(indices), 'type': 'SCALAR'}],
        'images': [{'uri': 'corners.png'}], 'samplers': [{'magFilter': 9728, 'minFilter': 9728, 'wrapS': 33071, 'wrapT': 33071}],
        'textures': [{'source': 0, 'sampler': 0}],
        'materials': [{'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'baseColorFactor': [1,1,1,1]}}],
        'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1, 'TEXCOORD_0': 2}, 'indices': 3, 'material': 0}]}]
    }
    return result, blob, image


def write_json(path, doc):
    path.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def write_glb(path, doc, blob, image):
    doc = copy.deepcopy(doc)
    blob += b'\0' * (-len(blob) % 4)
    doc['bufferViews'].append({'buffer': 0, 'byteOffset': len(blob), 'byteLength': len(image)})
    doc['images'] = [{'bufferView': len(doc['bufferViews']) - 1, 'mimeType': 'image/png'}]
    blob += image
    doc['buffers'] = [{'byteLength': len(blob)}]
    blob += b'\0' * (-len(blob) % 4)
    text = json.dumps(doc, ensure_ascii=False).encode('utf-8')
    text += b' ' * (-len(text) % 4)
    content = struct.pack('<II', len(text), 0x4e4f534a) + text + struct.pack('<II', len(blob), 0x004e4942) + blob
    path.write_bytes(struct.pack('<III', 0x46546c67, 2, 12 + len(content)) + content)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    TEST.mkdir(parents=True, exist_ok=True)
    quad = [(-.6,-.6,0,0,0,1,0,1), (.6,-.6,0,0,0,1,1,1), (.6,.6,0,0,0,1,1,0), (-.6,.6,0,0,0,1,0,0)]
    doc, blob, image = document(quad, [0,1,2,0,2,3])
    (TEST / 'mesh.bin').write_bytes(blob)
    (TEST / 'corners.png').write_bytes(image)
    write_json(TEST / 'quad.gltf', doc)
    write_glb(TEST / 'quad.glb', doc, blob, image)
    embedded = copy.deepcopy(doc)
    embedded['buffers'][0]['uri'] = 'data:application/octet-stream;base64,' + base64.b64encode(blob).decode()
    embedded['images'][0]['uri'] = 'data:image/png;base64,' + base64.b64encode(image).decode()
    write_json(TEST / 'embedded.gltf', embedded)
    for index_type in [5121, 5125]:
        variant, vb, vi = document(quad, [0,1,2,0,2,3], index_type)
        write_glb(TEST / f'indices-{index_type}.glb', variant, vb, vi)
    # 非索引网格以三角形顶点顺序保存。
    variant, vb, vi = document([quad[i] for i in [0,1,2,0,2,3]], [0,1,2,3,4,5])
    del variant['meshes'][0]['primitives'][0]['indices']
    write_glb(TEST / 'nonindexed.glb', variant, vb, vi)
    for name, edit in {
        'bad-offset': lambda d: d['accessors'][0].update(byteOffset=99999),
        'blend': lambda d: d['materials'][0].update(alphaMode='BLEND'),
        'mask': lambda d: d['materials'][0].update(alphaMode='MASK'),
        'extension': lambda d: d.update(extensionsRequired=['KHR_draco_mesh_compression']),
        'cycle': lambda d: d['nodes'][1].update(children=[0]),
        'shear': lambda d: d['nodes'][0].update(matrix=[1,0,0,0,.5,1,0,0,0,0,1,0,0,0,0,1]),
        'bad-index': lambda d: d['accessors'][3].update(count=9999),
        'morph': lambda d: d['meshes'][0]['primitives'][0].update(targets=[{'POSITION': 0}]),
        'uv1': lambda d: d['materials'][0]['pbrMetallicRoughness']['baseColorTexture'].update(texCoord=1),
        'matrix': lambda d: d['nodes'][0].update(matrix=[-2,0,0,0,0,3,0,0,0,0,4,0,1,2,3,1]),
        'quaternion': lambda d: d['nodes'][0].update(rotation=[0,0,math.sin(.3),math.cos(.3)]),
    }.items():
        variant = copy.deepcopy(doc)
        edit(variant)
        write_json(TEST / f'{name}.gltf', variant)

    # 自有低多边形“展示装置”：球面分片+底座，同一mesh的两个primitive使用不同材质。
    vertices, indices = [], []
    for ring in range(1, 8):
        latitude = math.pi * ring / 8
        for segment in range(16):
            angle = 2 * math.pi * segment / 16
            n = (math.sin(latitude)*math.cos(angle), math.cos(latitude), math.sin(latitude)*math.sin(angle))
            vertices.append((*n, *n, segment/16, ring/8))
    for ring in range(6):
        for segment in range(16):
            a = ring*16+segment
            b = ring*16+(segment+1)%16
            indices.extend([a,b,b+16,a,b+16,a+16])
    # 封闭两极，保持外侧逆时针绕序。
    top, bottom = len(vertices), len(vertices)+1
    vertices += [(0,1,0,0,1,0,.5,0), (0,-1,0,0,-1,0,.5,1)]
    for s in range(16):
        indices.extend([top,(s+1)%16,s, bottom,96+s,96+(s+1)%16])
    showcase, sb, si = document(vertices, indices)
    showcase['materials'].append({'pbrMetallicRoughness': {'baseColorFactor': [.12,.55,.9,1]}, 'doubleSided': True})
    second = copy.deepcopy(showcase['meshes'][0]['primitives'][0])
    second['material'] = 1
    showcase['meshes'].append({'primitives': [second]})
    # 球体切成两组索引，确保一个mesh内确实包含两个使用不同材质的primitive。
    # 底座仍引用原始完整索引，因此不需要复制顶点或修改二进制数据。
    first_count = (len(indices) // 6) * 3
    for offset, count in [(0, first_count), (first_count * 2, len(indices) - first_count)]:
        accessor = copy.deepcopy(showcase['accessors'][3])
        accessor.update(byteOffset=offset, count=count)
        showcase['accessors'].append(accessor)
    showcase['meshes'][0]['primitives'][0]['indices'] = 4
    globe_lower = copy.deepcopy(second)
    globe_lower['indices'] = 5
    showcase['meshes'][0]['primitives'].append(globe_lower)
    # 保留两个primitive的测试覆盖，同时节点给不同部件独立的局部变换。
    showcase['nodes'] = [{'name': 'assembly', 'children': [1,2]},
                         {'name': 'textured globe', 'mesh': 0, 'scale': [.65,.65,.65]},
                         {'name': 'blue pedestal', 'mesh': 1, 'translation': [0,-.75,0], 'scale': [.5,.15,.5]}]
    (OUT / 'mesh.bin').write_bytes(sb)
    (OUT / 'corners.png').write_bytes(si)
    write_json(OUT / 'showcase.gltf', showcase)
    write_glb(OUT / 'showcase.glb', showcase, sb, si)
    multi = copy.deepcopy(doc)
    multi['materials'].append({'pbrMetallicRoughness': {'baseColorFactor': [.1,.2,.3,1]}})
    part = copy.deepcopy(multi['meshes'][0]['primitives'][0]); part['material'] = 1
    multi['meshes'][0]['primitives'].append(part)
    write_json(TEST / 'multi.gltf', multi)
    factor = copy.deepcopy(doc)
    factor['images'][0]['uri'] = 'data:image/png;base64,' + base64.b64encode(png(True)).decode()
    factor['materials'][0]['pbrMetallicRoughness']['baseColorFactor'] = [.5,.5,.5,.2]
    write_json(TEST / 'factor.gltf', factor)


if __name__ == '__main__':
    main()
