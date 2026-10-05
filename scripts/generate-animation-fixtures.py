"""生成自有刚体动画装置和边界夹具。运行时读取GLB，不在示例中现场拼模型。"""
import copy
import math
import pathlib
import runpy
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = runpy.run_path(str(ROOT / 'scripts/generate-gltf-fixtures.py'))
OUT = ROOT / 'tests/fixtures/animation'
EXAMPLE = ROOT / 'examples/node_animation/assets'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    EXAMPLE.mkdir(parents=True, exist_ok=True)
    vertices, indices = [], []
    # 每面独立法线和UV，逆时针绕序；简单模型便于逐节点核对导入和动画空间。
    for normal, corners in [
        ((0,0,1), [(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]),
        ((0,0,-1), [(1,-1,-1),(-1,-1,-1),(-1,1,-1),(1,1,-1)]),
        ((1,0,0), [(1,-1,1),(1,-1,-1),(1,1,-1),(1,1,1)]),
        ((-1,0,0), [(-1,-1,-1),(-1,-1,1),(-1,1,1),(-1,1,-1)]),
        ((0,1,0), [(-1,1,1),(1,1,1),(1,1,-1),(-1,1,-1)]),
        ((0,-1,0), [(-1,-1,-1),(1,-1,-1),(1,-1,1),(-1,-1,1)])]:
        first = len(vertices)
        vertices.extend((*p, *normal, *uv) for p, uv in zip(corners, [(0,0),(1,0),(1,1),(0,1)]))
        indices.extend(first+i for i in [0,1,2,0,2,3])
    doc, blob, image = BASE['document'](vertices, indices)
    doc['materials'] = [
        {'pbrMetallicRoughness': {'baseColorFactor':[.04,.45,.8,1], 'metallicFactor':.7, 'roughnessFactor':.25}},
        {'pbrMetallicRoughness': {'baseColorFactor':[.6,.18,.03,1], 'metallicFactor':.8, 'roughnessFactor':.4}}]
    second = copy.deepcopy(doc['meshes'][0])
    second['primitives'][0]['material'] = 1
    doc['meshes'].append(second)
    # 文件顺序刻意不同于层级顺序，同时重复名称，防止播放器按名称或原索引误绑定。
    doc['nodes'] = [
        {'name':'part', 'mesh':0, 'scale':[1,.12,.25]},
        {'name':'rotor', 'translation':[0,1,0], 'children':[0,4]},
        {'name':'assembly', 'children':[1,3]},
        {'name':'part', 'mesh':1, 'translation':[0,.15,0], 'scale':[.6,.15,.6]},
        {'name':'part', 'mesh':1, 'scale':[.12,1,.25]}]
    doc['scenes'] = [{'nodes':[2]}]

    def accessor(kind, values):
        nonlocal blob
        components = {'SCALAR':1, 'VEC3':3, 'VEC4':4}[kind]
        blob += b'\0' * (-len(blob) % 4)
        data = struct.pack('<' + 'f'*len(values), *values)
        view = len(doc['bufferViews'])
        doc['bufferViews'].append({'buffer':0, 'byteOffset':len(blob), 'byteLength':len(data)})
        result = len(doc['accessors'])
        doc['accessors'].append({'bufferView':view, 'componentType':5126, 'count':len(values)//components, 'type':kind})
        blob += data
        return result

    times = accessor('SCALAR', [0,1,2,3,4])
    positions = accessor('VEC3', [0,1,0, 0,1.5,0, 0,2,0, 0,1.5,0, 0,1,0])
    rotations = accessor('VEC4', [v for k in range(5) for v in (0, math.sin(k*math.pi/4), 0, math.cos(k*math.pi/4))])
    scales = accessor('VEC3', [v for k in range(5) for v in (1 + (k%2)*.4, .12, .25)])
    doc['animations'] = [{'name':'Float and spin', 'samplers':[
        {'input':times, 'output':positions, 'interpolation':'LINEAR'},
        {'input':times, 'output':rotations, 'interpolation':'LINEAR'},
        {'input':times, 'output':scales, 'interpolation':'STEP'}], 'channels':[
        {'sampler':0, 'target':{'node':1, 'path':'translation'}},
        {'sampler':1, 'target':{'node':1, 'path':'rotation'}},
        {'sampler':2, 'target':{'node':0, 'path':'scale'}}]},
        {'name':'Spin only', 'samplers':[{'input':times, 'output':rotations}],
         'channels':[{'sampler':0, 'target':{'node':1, 'path':'rotation'}}]}]
    BASE['write_glb'](OUT / 'rigid.glb', doc, blob, image)
    BASE['write_glb'](EXAMPLE / 'kinetic.glb', doc, blob, image)
    for name, edit in {
        'cubic': lambda d: d['animations'][0]['samplers'][0].update(interpolation='CUBICSPLINE'),
        'morph': lambda d: d['animations'][0]['channels'][0]['target'].update(path='weights'),
        'duplicate': lambda d: d['animations'][0]['channels'].append(copy.deepcopy(d['animations'][0]['channels'][0])),
        'matrix': lambda d: d['nodes'][1].update(matrix=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]),
        'count': lambda d: d['accessors'][positions].update(count=4),
    }.items():
        variant = copy.deepcopy(doc)
        edit(variant)
        BASE['write_glb'](OUT / (name + '.glb'), variant, blob, image)
    for name, target, values in [('bad-time', times, [0,1,1,3,4]),
                                  ('bad-quat', rotations, [0,0,0,0])]:
        changed = bytearray(blob)
        start = doc['bufferViews'][doc['accessors'][target]['bufferView']]['byteOffset']
        struct.pack_into('<'+'f'*len(values), changed, start, *values)
        BASE['write_glb'](OUT / (name + '.glb'), doc, bytes(changed), image)


if __name__ == '__main__':
    main()
