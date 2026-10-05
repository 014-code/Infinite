"""生成可人工核对的小型PBR/后续动画回归资源，不覆盖历史预览夹具。"""
import copy
import pathlib
import runpy
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = runpy.run_path(str(ROOT / 'scripts/generate-gltf-fixtures.py'))
OUT = ROOT / 'tests/fixtures/gltf_advanced'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    quad = [(-.6,-.6,0,0,0,1,0,1), (.6,-.6,0,0,0,1,1,1),
            (.6,.6,0,0,0,1,1,0), (-.6,.6,0,0,0,1,0,0)]
    doc, blob, _ = BASE['document'](quad, [0,1,2,0,2,3])
    image = BASE['png'](True)
    # 复用同一图片同时作为颜色/数据纹理，专门检测GPU缓存是否包含颜色空间。
    material = doc['materials'][0]
    material['pbrMetallicRoughness'].update(metallicFactor=.7, roughnessFactor=.6,
        metallicRoughnessTexture={'index': 0}, baseColorFactor=[.5,.5,.5,.5])
    material.update(normalTexture={'index':0, 'scale':.8}, occlusionTexture={'index':0, 'strength':.4},
        emissiveTexture={'index':0}, emissiveFactor=[.1,.2,.3], doubleSided=True)

    def attribute(name, components, values):
        nonlocal blob
        blob += b'\0' * (-len(blob) % 4)
        data = struct.pack('<' + 'f' * len(values), *values)
        view = len(doc['bufferViews'])
        doc['bufferViews'].append({'buffer':0, 'byteOffset':len(blob), 'byteLength':len(data)})
        accessor = len(doc['accessors'])
        doc['accessors'].append({'bufferView':view, 'componentType':5126, 'count':4, 'type':f'VEC{components}'})
        doc['meshes'][0]['primitives'][0]['attributes'][name] = accessor
        blob += data

    attribute('COLOR_0', 4, [.5,.25,1,.5] * 4)
    attribute('TANGENT', 4, [1,0,0,1] * 4)
    BASE['write_glb'](OUT / 'pbr.glb', doc, blob, image)
    for mode in ['MASK', 'BLEND']:
        variant = copy.deepcopy(doc)
        variant['materials'][0].update(alphaMode=mode, alphaCutoff=.3)
        BASE['write_glb'](OUT / (mode.lower() + '.glb'), variant, blob, image)
    variant = copy.deepcopy(doc)
    variant['extensionsRequired'] = ['KHR_materials_unlit']
    variant['materials'][0]['extensions'] = {'KHR_materials_unlit': {}}
    BASE['write_glb'](OUT / 'unlit.glb', variant, blob, image)
    variant = copy.deepcopy(doc)
    variant['materials'][0]['normalTexture']['texCoord'] = 1
    BASE['write_glb'](OUT / 'bad-normal-uv.glb', variant, blob, image)
    variant = copy.deepcopy(doc)
    variant['materials'][0]['pbrMetallicRoughness']['roughnessFactor'] = -1
    BASE['write_glb'](OUT / 'bad-roughness.glb', variant, blob, image)


if __name__ == '__main__':
    main()
