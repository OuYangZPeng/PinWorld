"""Hall of Prayer for Good Harvests: approximate architectural study in metres.
Run with Blender --background --python build_hall_of_prayer.py.
Only writes assets beside this script; does not access other project assets.
"""
import bpy
import math
import json
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
for c in list(bpy.data.collections):
    if c.name != 'Collection':
        bpy.data.collections.remove(c)
B = {}
M = {}

def mat(name, color, metal=0, rough=.5):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    p = m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = (*color, 1)
    p.inputs['Metallic'].default_value = metal
    p.inputs['Roughness'].default_value = rough
    M[name] = m

for args in [
    ('marble', (.77,.79,.76)), ('carving', (.90,.88,.79)),
    ('red', (.46,.025,.015)), ('red_dark', (.24,.018,.012)),
    ('shadow', (.022,.028,.031)), ('teal', (.018,.24,.22)),
    ('paint_blue', (.018,.09,.25)), ('gold', (.83,.51,.095),.7,.26),
    ('tile', (.012,.075,.25),.22,.28), ('ridge', (.018,.145,.39),.25,.25),
    ('tile_edge', (.025,.21,.43),.18,.28), ('grout', (.19,.22,.23)),
    ('paving', (.36,.39,.40)), ('paving_alt', (.40,.42,.42))]:
    mat(*args)

GROUP = 'Site'
def mesh(key, vs, fs):
    verts, faces = B.setdefault((GROUP,key), ([],[]))
    n = len(verts)
    verts.extend(vs)
    faces.extend(tuple(n+i for i in f) for f in fs)

def box(key, c, size, angle=0):
    x,y,z = c
    a,b,h = [s/2 for s in size]
    co,si = math.cos(angle),math.sin(angle)
    vs = [(x+u*co-v*si,y+u*si+v*co,z+w) for u,v,w in
          [(-a,-b,-h),(a,-b,-h),(a,b,-h),(-a,b,-h),(-a,-b,h),(a,-b,h),(a,b,h),(-a,b,h)]]
    mesh(key,vs,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)])

def tube(key, points, radius, sides=8):
    vs=[]
    for i,p in enumerate(points):
        tangent=(Vector(points[min(i+1,len(points)-1)])-Vector(points[max(0,i-1)])).normalized()
        ref=Vector((0,0,1)) if abs(tangent.z)<.95 else Vector((1,0,0))
        u=tangent.cross(ref).normalized(); v=tangent.cross(u)
        vs.extend(tuple(Vector(p)+radius*(math.cos(j*math.tau/sides)*u+math.sin(j*math.tau/sides)*v)) for j in range(sides))
    fs=[(i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j) for i in range(len(points)-1) for j in range(sides)]
    fs.extend([tuple(reversed(range(sides))),tuple((len(points)-1)*sides+j for j in range(sides))])
    mesh(key,vs,fs)

def polar(r,a,z):
    return (r*math.cos(a),r*math.sin(a),z)

def lathe(key, profile, n=192):
    vs=[polar(r,j*math.tau/n,z) for r,z in profile for j in range(n)]
    fs=[(i*n+j,i*n+(j+1)%n,(i+1)*n+(j+1)%n,(i+1)*n+j) for i in range(len(profile)-1) for j in range(n)]
    fs.extend([tuple(reversed(range(n))),tuple((len(profile)-1)*n+j for j in range(n))])
    mesh(key,vs,fs)

def ring(key,r,z,thick):
    tube(key,[polar(r,j*math.tau/192,z) for j in range(193)],thick)

def column(key,r,a,z,h,width):
    tube(key,[polar(r,a,z),polar(r,a,z+h)],width,12)

def radial_box(key,r,a,z,size):
    box(key,polar(r,a,z),size,a)

# Real paving gaps, grouped into two meshes.
box('grout',(0,0,-.24),(100,100,.4))
for x in range(-25,25):
    for y in range(-25,25):
        if math.hypot(x*2+1,y*2+1)<29:
            continue
        box('paving' if (x*17+y*7)%5 else 'paving_alt',(x*2+1,y*2+1,-.025),(1.975,1.975,.09))

# Concentric terraces; rail gaps and steps share exactly the same four axes.
for level,r in enumerate([28,24,20]):
    GROUP='Terrace_%d'%(level+1)
    z=level*1.5; top=z+1.5
    lathe('marble',[(r,z),(r,z+1.35)])
    lathe('carving',[(r+.16,z+.08),(r+.16,z+.25),(r,z+.29)])
    lathe('carving',[(r,top-.18),(r+.25,top-.16),(r+.25,top)])
    for h in [.54,1.02]: ring('carving',r+.02,z+h,.035)
    rr=r-.30
    gap=math.asin(3.15/rr)
    for quarter in range(4):
        start=quarter*math.pi/2+gap
        end=(quarter+1)*math.pi/2-gap
        count=math.ceil((end-start)*rr/1.65)
        for i in range(count+1):
            a=start+(end-start)*i/count
            radial_box('carving',rr,a,top+.63,(.28,.28,1.26))
            column('carving',rr,a,top+1.22,.15,.21)
            # Lotus-like finial, consolidated with the stone geometry.
            p=polar(rr,a,0)
            for h,rad in [(1.36,.17),(1.48,.12)]:
                tube('carving',[(p[0],p[1],top+h-.07),(p[0],p[1],top+h+.07)],rad,8)
            if i<count:
                b=start+(end-start)*(i+1)/count
                for h in [.23,.93]:
                    tube('carving',[polar(rr,a,top+h),polar(rr,b,top+h)],.09)
                for k in range(1,5):
                    aa=a+(b-a)*k/5
                    column('marble',rr,aa,top+.25,.65,.055)
                aa=(a+b)/2
                radial_box('carving',rr,aa,top+.58,(.13,.32,.44))
    for q in range(4):
        a=q*math.pi/2
        # Nine treads, outer foot on lower terrace; inner end buried in rim.
        for s in range(10):
            h=(s+1)*.15
            radial_box('marble',r+3.40-s*.36,a,z+h/2,(.39,6.0,h))
        for side in [-1,1]:
            u=Vector((math.cos(a),math.sin(a),0)); v=Vector((-math.sin(a),math.cos(a),0))
            p=u*(r+3.60)+v*(side*3.04)+Vector((0,0,z+.27))
            t=u*(r-.10)+v*(side*3.04)+Vector((0,0,top+.27))
            tube('carving',[p,t],.13)
        if q==3:
            # South imperial way, above treads with low scroll relief.
            y0=-r-3.60; y1=-r+.12
            mesh('carving',[(-.75,y0,z+.17),(.75,y0,z+.17),(.75,y1,top+.18),(-.75,y1,top+.18)],[(0,1,2,3)])
            for side in [-1,1]:
                tube('marble',[(side*.72,y0,z+.23),(side*.72,y1,top+.24)],.055)
            pts=[(.34*math.sin(t*math.tau*2.4),y0+.12+t*(y1-y0-.24),z+.26+t*1.49) for t in [i/48 for i in range(49)]]
            tube('marble',pts,.045,6)

GROUP='Circular_hall'
lathe('carving',[(13.8,4.5),(13.8,4.8)])
lathe('red_dark',[(12.45,4.8),(12.45,12.6)])
# 24 equal bays: red framing with dark inset and fine gold/wood lattice.
for i in range(24):
    a=i*math.tau/24
    column('carving',13.05,a,4.8,.35,.40)
    column('red',13.05,a,5.15,7.0,.29)
    column('gold',13.05,a,11.65,.10,.31)
    mid=a+math.pi/24
    radial_box('shadow',12.49,mid,8.55,(.10,2.75,6.0))
    for side in [-1,1]:
        # Tangentially spaced local coordinates for rectangular bay frames.
        c=Vector(polar(12.58,mid,8.55))+Vector((-math.sin(mid),math.cos(mid),0))*side*1.37
        box('red',c,(.18,.13,6.15),mid)
    for h in [5.55,6.75,10.95,11.55]: radial_box('red',12.60,mid,h,(.18,2.78,.15))
    radial_box('red',12.60,mid,6.05,(.16,2.65,.85))
    for k in range(-5,6):
        c=Vector(polar(12.65,mid,8.83))+Vector((-math.sin(mid),math.cos(mid),0))*k*.235
        box('gold' if k in [-5,5] else 'red',c,(.08,.045,4.0),mid)
    for h in range(14): radial_box('red',12.67,mid,6.95+h*.29,(.08,2.5,.045))

# Three tiers, deliberately round rather than rectangular pavilion roofs.
# Concave roof profiles flatten then turn upward at the outer eaves.
for tier,(outer,inner,base,rise) in enumerate([(16.1,9.8,13.5,5.3),(13.0,7.2,20.0,5.0),(10.15,.38,26.0,8.0)]):
    GROUP='Roof_%d'%(tier+1)
    support=outer-1.75
    lathe('red',[(support-.18,base-1.9),(support-.18,base-.35)])
    lathe('paint_blue',[(support+.05,base-1.05),(support+.05,base-.30)])
    for h in [base-1.04,base-.34]: ring('gold',support+.10,h,.045)
    for j in range(72):
        a=j*math.tau/72
        radial_box('teal',support+.14,a,base-.69,(.09,.70,.34))
        radial_box('gold',support+.20,a,base-.69,(.06,.22,.12))
        # Three interlocking bracket arms below the roof.
        for k in range(3):
            radial_box('teal' if k!=1 else 'paint_blue',support+.23+k*.29,a,base-.29+k*.18,(.8,.24+k*.17,.18))
        radial_box('red',support+.13,a,base-.78,(.35,.18,.73))
    def roof_point(t):
        # t=0 outside; minimum slightly inward gives the raised lip.
        return (outer-(outer-inner)*t,base+rise*(t**1.65)-.30*math.sin(math.pi*min(t/.20,1)) if t<.20 else base+rise*t**1.65)
    profile=[roof_point(i/48) for i in range(49)]
    lathe('tile',[(outer,base-.18)]+profile+[(inner,base+rise-.16)],256)
    ring('tile_edge',outer,base,.105)
    ring('gold',outer-.025,base-.19,.035)
    # Long radial convex tile rolls plus transverse courses.
    count=[240,208,176][tier]
    for j in range(count):
        a=j*math.tau/count
        tube('ridge',[polar(r,a,z+.055) for r,z in profile],.060 if tier<2 else .052,6)
        column('tile_edge',outer,a,base-.13,.12,.085)
    for k in range(1,23):
        r,z=roof_point(k/24)
        ring('ridge',r,z+.027,.025)

GROUP='Golden_finial'
lathe('gold',[(.80,33.85),(.86,34.04),(.70,34.22),(.51,34.30),(.66,34.65),(.67,35.03),(.47,35.38),(.23,35.55),(.20,36.05),(.06,36.42),(0,36.56)],96)
ring('gold',.79,34.06,.08)

# The south-facing blue plaque and packed Chinese typeface.
GROUP='South_plaque'
box('gold',(0,-14.43,12.17),(3.4,.20,1.20))
box('paint_blue',(0,-14.56,12.17),(3.22,.12,1.03))
font_path=Path('C:/Windows/Fonts/msyhbd.ttc')
if font_path.exists():
    font=bpy.data.fonts.load(str(font_path))
    curve=bpy.data.curves.new('祈年殿 inscription','FONT')
    curve.body='祈年殿'; curve.font=font; curve.align_x='CENTER'; curve.align_y='CENTER'
    curve.size=.79; curve.extrude=.008
    obj=bpy.data.objects.new('祈年殿 · south inscription',curve)
    bpy.context.scene.collection.objects.link(obj)
    obj.location=(0,-14.64,12.17); obj.rotation_euler=(math.pi/2,0,0)
    obj.data.materials.append(M['gold'])

# Material-batched geometry, organized by architectural tier.
for (group,key),(vs,fs) in B.items():
    collection=bpy.data.collections.get(group)
    if collection is None:
        collection=bpy.data.collections.new(group); bpy.context.scene.collection.children.link(collection)
    data=bpy.data.meshes.new(group+'_'+key)
    data.from_pydata(vs,[],fs); data.materials.append(M[key]); data.update()
    ob=bpy.data.objects.new(group+'_'+key,data); collection.objects.link(ob)
    if key in ['tile','ridge','tile_edge','gold'] and group.startswith(('Roof','Golden')):
        for p in data.polygons: p.use_smooth=True

scene=bpy.context.scene
scene.unit_settings.system='METRIC'
scene.unit_settings.scale_length=1
scene['Asset']='Beijing Temple of Heaven · Hall of Prayer for Good Harvests'
scene['Accuracy']='Approximate exterior reconstruction, not survey data. Simplified dougong, relief and lattice; no interior.'
scene.render.engine='CYCLES'
scene.cycles.samples=32
scene.cycles.use_denoising=True
scene.render.resolution_x=1500; scene.render.resolution_y=1500; scene.render.resolution_percentage=100
scene.world.color=(.35,.35,.35)
scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.38,.49,.65,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.45

def aim(obj,point): obj.rotation_euler=(Vector(point)-obj.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.light_add(type='SUN',location=(20,-30,50))
sun=bpy.context.object; sun.name='Warm afternoon sun'; sun.data.energy=2.6; sun.data.angle=.15
sun.rotation_euler=(math.radians(25),math.radians(-20),math.radians(-28))
bpy.ops.object.light_add(type='AREA',location=(10,-35,32))
fill=bpy.context.object; fill.name='Soft facade fill'; fill.data.energy=2600; fill.data.shape='DISK'; fill.data.size=25; aim(fill,(0,0,14))
bpy.ops.object.camera_add(location=(49,-83,49))
camera=bpy.context.object; camera.name='Presentation south east'; aim(camera,(0,0,16))
camera.data.type='ORTHO'; camera.data.ortho_scale=77; camera.data.lens=50
scene.camera=camera
scene.view_settings.view_transform='AgX'
scene.render.image_settings.file_format='PNG'
scene.render.filepath=str(ROOT/'preview.png')
# Whole asset visible on opening; material colors also work without render.
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            space=area.spaces.active
            space.clip_end=1000
            space.shading.type='MATERIAL'
            space.region_3d.view_distance=85
            space.region_3d.view_location=(0,0,16)
            space.region_3d.view_rotation=camera.rotation_euler.to_quaternion()
            space.region_3d.view_perspective='CAMERA'
bpy.ops.object.select_all(action='DESELECT')
bpy.context.view_layer.objects.active=None
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'HallOfPrayer.blend'))
report={'blender':bpy.app.version_string,'objects':len(scene.objects),'mesh_faces':sum(len(o.data.polygons) for o in scene.objects if o.type=='MESH'),'mesh_vertices':sum(len(o.data.vertices) for o in scene.objects if o.type=='MESH'),'font_packed':bool(font_path.exists()),'height_m':36.56}
(ROOT/'build_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
bpy.ops.render.render(write_still=True)
print('HALL_OF_PRAYER_COMPLETE',json.dumps(report))
