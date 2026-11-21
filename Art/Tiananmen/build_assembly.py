"""Approximate Tiananmen, metres. Run Blender --background --python this file.
No portrait or invented signage. Five open vaulted passages, not decals.
"""
import bpy
import math
import json
import hashlib
from pathlib import Path
from mathutils import Vector
ROOT = Path(__file__).resolve().parent
PROJECT = ROOT.parents[1]
OUT = PROJECT/'Content/UnityMigrationData/Tiananmen'
OUT.mkdir(parents=True, exist_ok=True)
(OUT/'meshes').mkdir(exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
M = {}
for name, color, roughness, metallic in [
    ('wall_red', (.48,.035,.018,1),.85,0), ('wood_red',(.32,.018,.012,1),.6,0),
    ('tile_gold',(.8,.43,.035,1),.3,0), ('ridge_gold',(.95,.62,.08,1),.35,0),
    ('stone',(.65,.63,.55,1),.8,0), ('paving',(.28,.29,.28,1),.95,0),
    ('paint_blue',(.025,.13,.25,1),.5,0), ('paint_green',(.025,.24,.12,1),.55,0),
    ('window',(.045,.025,.018,1),.7,0)]:
    mat=bpy.data.materials.new(name); mat.diffuse_color=color; mat.use_nodes=True
    bsdf=mat.node_tree.nodes['Principled BSDF']
    bsdf.inputs['Base Color'].default_value=color
    bsdf.inputs['Roughness'].default_value=roughness
    bsdf.inputs['Metallic'].default_value=metallic
    M[name]=mat
parts={}
def emit(part, material, vertices, faces):
    vs,fs=parts.setdefault(part,{}).setdefault(material,([],[]))
    base=len(vs); vs.extend(vertices); fs.extend(tuple(base+i for i in f) for f in faces)
def box(p,m,x,y,z,sx,sy,sz):
    v=[(x+a*sx/2,y+b*sy/2,z+c*sz/2) for a,b,c in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
    emit(p,m,v,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)])
def cylinder(p,m,x,y,z,r,h,n=12):
    v=[(x+r*math.cos(i*math.tau/n),y+r*math.sin(i*math.tau/n),z+k*h) for k in (0,1) for i in range(n)]
    f=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    emit(p,m,v,f)
box('Plaza','paving',0,-14,-.3,88,57,.6)
# Platform 66m wide, 28m deep, 12m high. Five vaulted openings.
centers=[-23,-11.5,0,11.5,23]; radii=[2,2.1,2.6,2.1,2]
left=-33
for i,(cx,r) in enumerate(zip(centers,radii)):
    right=cx-r
    box('Wall_Pier_%d'%i,'wall_red',(left+right)/2,0,6,right-left,28,12)
    # Extrude each arch wedge all the way through wall; curved inner soffit is real geometry.
    v=[]; f=[]
    for j in range(25):
        t=math.pi-j*math.pi/24; x=cx+r*math.cos(t); z=4.2+r*math.sin(t)
        v.extend([(x,-14,z),(x,14,z),(x,-14,12),(x,14,12)])
    for j in range(24):
        a=j*4; b=a+4
        f.extend([(a,b,b+1,a+1),(a+2,a+3,b+3,b+2),(a,a+2,b+2,b),(a+1,b+1,b+3,a+3)])
    f.extend([(0,1,3,2),(96,98,99,97)])
    emit('Arch_Vault_%d'%i,'wall_red',v,f)
    left=cx+r
box('Wall_Pier_5','wall_red',(left+33)/2,0,6,33-left,28,12)
# Deck must not seal openings below roof of vault.
box('Terrace_Deck','stone',0,0,12.2,67,29,.4)
for side in (-1,1):
    p='Balustrade_'+('Front' if side<0 else 'Back')
    for x in range(-33,34,2):
        box(p,'stone',x,side*14,13, .3,.3,1.4)
        cylinder(p,'stone',x,side*14,13.7,.24,.25)
    for z in (12.65,13.5): box(p,'stone',0,side*14,z,66,.2,.18)
    p='Balustrade_'+('West' if side<0 else 'East')
    for y in range(-12,13,2): box(p,'stone',side*33,y,13,.3,.3,1.4)
    for z in (12.65,13.5): box(p,'stone',side*33,0,z,.2,28,.18)
box('Hall_Shell','wood_red',0,0,15.7,49,12,6.6)
for side in (-1,1):
    p='Columns_'+('Front' if side<0 else 'Back')
    for i in range(11):
        x=(i-5)*4.8
        cylinder(p,'stone',x,side*7.5,12.4,.52,.4)
        cylinder(p,'wood_red',x,side*7.5,12.8,.32,5.8)
    p='Doors_'+('Front' if side<0 else 'Back')
    for i in range(10):
        x=(i-4.5)*4.8
        box(p,'window',x,side*6.05,15.3,3.9,.15,4.8)
        for k in range(7): box(p,'wood_red',x+(k-3)*.55,side*6.18,15.3,.09,.12,4.8)
        for z in (13.2,14,15,16,17.4): box(p,'wood_red',x,side*6.2,z,3.9,.12,.1)
    p='Painted_Beams_'+('Front' if side<0 else 'Back')
    box(p,'paint_blue',0,side*7.5,18.3,51,.65,.65)
    for i in range(21):
        x=(i-10)*2.4
        box(p,'paint_green',x,side*7.85,18.3,1.8,.08,.35)
        box(p,'ridge_gold',x,side*7.91,18.3,.65,.06,.13)
        for k in range(3): box(p,'paint_green',x,side*(7.6+k*.3),18.7+k*.22,.9-k*.16,.85,.18)
# Hip lower eave and upper hip-and-gable roof: slopes end in raised corners.
def roof(p,hx,hy,z,rise,topx,topy):
    # Four trapezoidal surfaces. Rows follow curved profile, ribs give tile rhythm.
    outer=[(-hx,-hy),(hx,-hy),(hx,hy),(-hx,hy)]
    inner=[(-topx,-topy),(topx,-topy),(topx,topy),(-topx,topy)]
    for s in range(4):
        a,b=outer[s],outer[(s+1)%4]; c,d=inner[s],inner[(s+1)%4]
        n=max(12,int(Vector((b[0]-a[0],b[1]-a[1])).length/.45))
        v=[]
        for row in range(9):
            t=row/8
            for col in range(n+1):
                u=col/n
                x=(a[0]*(1-u)+b[0]*u)*(1-t)+(c[0]*(1-u)+d[0]*u)*t
                y=(a[1]*(1-u)+b[1]*u)*(1-t)+(c[1]*(1-u)+d[1]*u)*t
                zz=z+rise*t**1.65+.85*(abs(2*u-1)**8)*(1-t)**3
                v.append((x,y,zz))
        f=[]
        for row in range(8):
            for col in range(n):
                k=row*(n+1)+col; f.append((k,k+1,k+n+2,k+n+1))
        emit(p,'tile_gold',v,f)
        # Individual broad raised ribs, kept owned by roof assembly, no tile pieces.
        for col in range(n+1):
            for row in range(8):
                a1=Vector(v[row*(n+1)+col]); b1=Vector(v[(row+1)*(n+1)+col])
                delta=Vector((.045,0,.035)) if s%2==0 else Vector((0,.045,.035))
                emit(p,'ridge_gold',[tuple(a1-delta),tuple(a1+delta),tuple(b1+delta),tuple(b1-delta)],[(0,1,2,3)])
roof('Roof_Lower',28,10,19,3.0,22,5.2)
box('Upper_Clerestory','wood_red',0,0,21.7,44,10.4,2.6)
roof('Roof_Upper',25,8,23,5,18,0.08)
# Gable infills and ridge caps, structurally separate.
for side in (-1,1):
    emit('Gable_'+('West' if side<0 else 'East'),'wood_red',[(side*18,-5,24),(side*18,5,24),(side*18,0,28)],[(0,1,2)])
box('Ridge_Crown','ridge_gold',0,0,28.15,37,.38,.4)
for x in (-18.5,18.5):
    for j in range(5): box('Ridge_Crown','ridge_gold',x+math.copysign(j*.15,x),0,28.35+j*.25,.35,.5,.35)
# Blank central panel intentionally has no text, flag, or fabricated portrait.
box('Central_Blank_Panel','stone',0,-14.08,9.2,3.1,.16,3.3)
box('Central_Blank_Panel','wood_red',0,-14.2,9.2,2.8,.12,3)
assert len(parts)==31, (len(parts),list(parts))
# Independent unsplit source: material batches, identical primitives and slots.
for material in M:
    vs=[]; fs=[]
    for bucket in parts.values():
        if material not in bucket: continue
        v,f=bucket[material]; base=len(vs); vs.extend(v); fs.extend(tuple(base+i for i in face) for face in f)
    mesh=bpy.data.meshes.new(material); mesh.from_pydata(vs,[],fs); mesh.materials.append(M[material]); mesh.update()
    obj=bpy.data.objects.new('Source_'+material,mesh); bpy.context.scene.collection.objects.link(obj)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'
scene.world.color=(.35,.35,.35)
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Tiananmen.blend'))
for obj in list(scene.objects): bpy.data.objects.remove(obj,do_unlink=True)
ns={'M':M}
manifest={'schemaVersion':1,'level':'Tiananmen','panelEntries':[],'objects':[],'materials':[{'id':n,'color':dict(zip('rgba',m.diffuse_color))} for n,m in M.items()],'doors':[]}
report={'sourceBuilderSha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'parts':[],'ownership':'Each primitive assigned once. No equivalent pieces assumed.','simplification':'Approximate non-survey model; blank central panel, no signage or portrait; no bridges or huabiao.'}
reference=(ROOT.parent/'TempleOfHeaven/build_assembly.py').read_text(encoding='utf-8')
a=reference.index('for name, materials in sorted(parts.items()):'); b=reference.index('scene = bpy.context.scene\nscene.world.color')
shared=reference[a:b].replace("'HallOfPrayer/'","'Tiananmen/'").replace("ROOT/'HallOfPrayer.blend'","ROOT/'Tiananmen.blend'").replace("'CardinalStairs' if name.startswith('Stairs_') else ''","''").replace("name.startswith(('Roof_', 'Finial'))","False")
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Tiananmen_Assembly.blend'))
exec(compile(shared,str(__file__)+':export','exec'),globals())
bpy.ops.object.light_add(type='SUN'); bpy.context.object.rotation_euler=(.5,-.4,-.4); bpy.context.object.data.energy=3
bpy.ops.object.camera_add(location=(78,-115,66)); camera=bpy.context.object
camera.rotation_euler=(Vector((0,-2,12))-camera.location).to_track_quat('-Z','Y').to_euler(); camera.data.type='ORTHO'; camera.data.ortho_scale=107; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=24
scene.render.resolution_x=1400; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'assembly_preview.png')
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.region_3d.view_perspective='CAMERA'; area.spaces.active.shading.color_type='MATERIAL'; area.spaces.active.clip_end=1000
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Tiananmen_Assembly.blend'))
bpy.ops.render.render(write_still=True)
print('TIANANMEN_ASSEMBLY_PASS',len(parts),flush=True)
