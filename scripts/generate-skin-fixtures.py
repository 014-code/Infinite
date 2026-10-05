"""生成两关节带状模型，绑定姿态/弯曲结果可用手算核对。"""
import copy
import math
import pathlib
import runpy
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = runpy.run_path(str(ROOT / 'scripts/generate-gltf-fixtures.py'))
OUT = ROOT / 'tests/fixtures/skin'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    vertices = [(x,y,0, 0,0,1, (x+.2)/.4,y/2) for y in [0,.5,1,1.5,2] for x in [-.2,.2]]
    indices = [i for row in range(4) for i in [row*2,row*2+1,row*2+3,row*2,row*2+3,row*2+2]]
    doc, blob, image = BASE['document'](vertices, indices)

    def accessor(kind, components, values, component_type=5126):
        nonlocal blob
        blob += b'\0' * (-len(blob) % 4)
        code = 'f' if component_type == 5126 else 'H'
        encoded = struct.pack('<' + code*len(values), *values)
        view = len(doc['bufferViews'])
        doc['bufferViews'].append({'buffer':0, 'byteOffset':len(blob), 'byteLength':len(encoded)})
        result = len(doc['accessors'])
        doc['accessors'].append({'bufferView':view, 'componentType':component_type, 'count':len(values)//components, 'type':kind})
        blob += encoded
        return result

    joint_attr = accessor('VEC4', 4, [0,1,0,0]*len(vertices), 5123)
    weights = [value for row in range(5) for _ in range(2) for value in (1-row/4, row/4, 0, 0)]
    weight_attr = accessor('VEC4', 4, weights)
    doc['meshes'][0]['primitives'][0]['attributes'].update(JOINTS_0=joint_attr, WEIGHTS_0=weight_attr)
    inverse = accessor('MAT4', 16, [1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1] +
        [1,0,0,0,0,1,0,0,0,0,1,0,0,-1,0,1])
    doc['nodes'] = [{'name':'root', 'children':[1,3]},
        {'name':'joint base', 'children':[2]}, {'name':'joint tip', 'translation':[0,1,0]},
        {'name':'ribbon', 'mesh':0, 'skin':0}]
    doc['skins'] = [{'joints':[1,2], 'inverseBindMatrices':inverse, 'skeleton':1}]
    times = accessor('SCALAR', 1, [0,1,2])
    rotation = accessor('VEC4', 4, [0,0,0,1, 0,0,math.sin(math.pi/4),math.cos(math.pi/4), 0,0,0,1])
    doc['animations'] = [{'name':'Bend', 'samplers':[{'input':times,'output':rotation}],
        'channels':[{'sampler':0, 'target':{'node':2,'path':'rotation'}}]}]
    doc['materials'] = [{'pbrMetallicRoughness': {'baseColorFactor':[.1,.5,.8,1],
        'metallicFactor':0, 'roughnessFactor':.5}, 'doubleSided':True}]
    BASE['write_glb'](OUT / 'ribbon.glb', doc, blob, image)
    for name, target, values, code in [('bad-joint',joint_attr,[0,2,0,0],'H'),
                                      ('bad-weight',weight_attr,[0,0,0,0],'f'),
                                      ('bad-matrix',inverse,[float('nan')],'f')]:
        changed = bytearray(blob)
        offset = doc['bufferViews'][doc['accessors'][target]['bufferView']]['byteOffset']
        struct.pack_into('<'+code*len(values), changed, offset, *values)
        BASE['write_glb'](OUT / (name+'.glb'), doc, bytes(changed), image)
    variant = copy.deepcopy(doc)
    del variant['meshes'][0]['primitives'][0]['attributes']['WEIGHTS_0']
    BASE['write_glb'](OUT / 'missing-weights.glb', variant, blob, image)
    variant = copy.deepcopy(doc)
    variant['skins'][0]['joints'] = [1,1]
    BASE['write_glb'](OUT / 'duplicate-joint.glb', variant, blob, image)
    variant = copy.deepcopy(doc)
    del variant['skins'][0]['inverseBindMatrices']
    BASE['write_glb'](OUT / 'identity-bind.glb', variant, blob, image)
    variant = copy.deepcopy(doc)
    del variant['meshes'][0]['primitives'][0]['attributes']['NORMAL']
    BASE['write_glb'](OUT / 'generated-normal.glb', variant, blob, image)


if __name__ == '__main__':
    main()
