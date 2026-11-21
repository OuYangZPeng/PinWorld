"""Procedural Taihe Hall presentation model, not a survey reconstruction.
Run: blender --background --python build_taihe.py
"""
import bpy
import math
import random
import json
from pathlib import Path
from mathutils import Vector, Quaternion

ROOT = Path(__file__).resolve().parent
random.seed(14)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
for block in list(bpy.data.materials):
    bpy.data.materials.remove(block)


def material(name, color, metallic=0.0, roughness=0.55):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Metallic'].default_value = metallic
    bsdf.inputs['Roughness'].default_value = roughness
    return m

M = {
    'stone': material('Warm white marble', (0.74, .72, .65)),
    'stone_light': material('Carved white marble', (.88, .86, .77)),
    'stone_dark': material('Marble joints', (.39, .40, .37)),
    'red': material('Vermilion lacquer', (.43, .032, .021), .08, .32),
    'darkred': material('Deep red framing', (.22, .018, .013)),
    'teal': material('Blue green painted beams', (.017, .19, .20)),
    'blue': material('Azurite blue details', (.018, .095, .25)),
    'gold': material('Gilded architectural details', (.82, .45, .075), .55, .3),
    'dark': material('Interior shadow', (.022, .015, .012)),
    'bronze': material('Patinated bronze', (.10, .17, .115), .72, .36),
    'paving': material('Courtyard grey stone', (.35, .37, .36)),
}
for i in range(6):
    M['tile'+str(i)] = material('Amber glazed tile %02d' % i,
                               (.70+i*.025, .30+i*.018, .028+i*.006), .16, .29)
# Aggregate simple geometry by material: no modifier stacks, few draw objects.
B = {}

def meshpart(key, verts, faces):
    vs, fs = B.setdefault(key, ([], []))
    n = len(vs)
    vs.extend(verts)
    fs.extend(tuple(n+i for i in f) for f in faces)


def box(key, center, size):
    x,y,z = center
    a,b,c = (v/2 for v in size)
    v = [(x+sx*a,y+sy*b,z+sz*c) for sx,sy,sz in
         [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),
          (-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
    meshpart(key,v,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)])


def tube(key, points, radius, sides=8, end_radius=None):
    verts=[]
    for i,p in enumerate(points):
        tangent=Vector(points[min(i+1,len(points)-1)])-Vector(points[max(0,i-1)])
        tangent.normalize()
        ref=Vector((0,0,1)) if abs(tangent.z)<.95 else Vector((1,0,0))
        u=tangent.cross(ref).normalized()
        v=tangent.cross(u).normalized()
        r=radius if end_radius is None else radius+(end_radius-radius)*i/(len(points)-1)
        for j in range(sides):
            verts.append(tuple(Vector(p)+r*(math.cos(j*math.tau/sides)*u+math.sin(j*math.tau/sides)*v)))
    faces=[]
    for i in range(len(points)-1):
        for j in range(sides):
            k=i*sides+j
            faces.append((k,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,k+sides))
    faces.extend([tuple(reversed(range(sides))),tuple((len(points)-1)*sides+j for j in range(sides))])
    meshpart(key,verts,faces)


def cylinder(key, x,y,z,r,h,sides=12):
    tube(key,[(x,y,z),(x,y,z+h)],r,sides)


def sphere(key,c,r):
    verts=[]
    for i in range(9):
        t=math.pi*i/8
        for j in range(12):
            p=math.tau*j/12
            verts.append((c[0]+r*math.sin(t)*math.cos(p),c[1]+r*math.sin(t)*math.sin(p),c[2]+r*math.cos(t)))
    meshpart(key,verts,[(i*12+j,i*12+(j+1)%12,(i+1)*12+(j+1)%12,(i+1)*12+j) for i in range(8) for j in range(12)])

# Courtyard: real joints and lightly varied slabs, surrounding low plinth.
box('stone_dark',(0,0,-.38),(124,104,.6))
for x in range(-30,31):
    for y in range(-25,26):
        box('paving',(x*2,y*2,-.035+random.uniform(-.009,.009)),(1.96,1.96,.12))
box('stone',(0,0,-.19),(126,106,.22))

# Three stacked marble terraces and their mouldings.
for level,(w,d) in enumerate([(70,48),(63,41),(56,34)]):
    z=level*1.2
    box('stone',(0,0,z+.55),(w,d,1.1))
    box('stone_light',(0,0,z+.12),(w+.25,d+.25,.22))
    box('stone_light',(0,0,z+1.13),(w+.6,d+.6,.20))
    # Horizontal wall courses and vertical panel joints.
    for h in [.36,.73]:
        box('stone_dark',(0,-d/2-.006,z+h),(w,.012,.018))
        box('stone_dark',(w/2+.006,0,z+h),(.012,d,.018))
    for x in range(-int(w/2)+2,int(w/2),3):
        box('stone_dark',(x,-d/2-.012,z+.60),(.018,.015,.88))


def railing_segment(a,b,z):
    length=(Vector(b)-Vector(a)).length
    n=max(1,round(length/2.1))
    for i in range(n+1):
        p=Vector(a).lerp(Vector(b),i/n)
        box('stone_light',(p.x,p.y,z+.56),(.29,.29,1.12))
        box('stone_light',(p.x,p.y,z+.99),(.38,.38,.12))
        sphere('stone_light',(p.x,p.y,z+1.23),.23)
        if i<n:
            q=Vector(a).lerp(Vector(b),(i+.5)/n)
            direction=(Vector(b)-Vector(a)).normalized()
            along=length/n-.29
            size=(along,.18,.55) if abs(direction.x)>.5 else (.18,along,.55)
            box('stone',(q.x,q.y,z+.43),size)
            for h in [.18,.79]:
                tube('stone_light',[(p.x,p.y,z+h),tuple(Vector(a).lerp(Vector(b),(i+1)/n))+ (z+h,)],.085,8)

for level,(w,d) in enumerate([(70,48),(63,41),(56,34)]):
    z=(level+1)*1.2
    # Three front stair breaks; one centered on each side.
    cuts=[(-w/2,-23),(-17,-6),(6,17),(23,w/2)]
    for a,b in cuts:
        if b>a: railing_segment((a,-d/2),(b,-d/2),z)
    railing_segment((-w/2,d/2),(w/2,d/2),z)
    for x in [-w/2,w/2]:
        railing_segment((x,-d/2),(x,-3),z)
        railing_segment((x,3),(x,d/2),z)
    # Each flight lands on next lower terrace.
    for cx,width in [(0,12),(-20,6),(20,6)]:
        for step in range(8):
            h=(step+1)*.15
            y=-d/2-3.05+step*.39
            box('stone_light',(cx,y,level*1.2+h/2),(width,.42,h))
        # Sloping stone stair cheeks.
        for sx in [cx-width/2,cx+width/2]:
            tube('stone_light',[(sx,-d/2-3.3,level*1.2+.25),(sx,-d/2,(level+1)*1.2+.25)],.16,8)
    for side in [-1,1]:
        for step in range(8):
            h=(step+1)*.15
            x=side*(w/2+3.05-step*.39)
            box('stone_light',(x,0,level*1.2+h/2),(.42,6,h))
    # Central imperial ramp: sloped stone relief, no flat floating slab.
    y0=-d/2-3.35; y1=-d/2+.1; z0=level*1.2+.18; z1=(level+1)*1.2+.18
    meshpart('stone', [(-1.4,y0,z0),(1.4,y0,z0),(1.4,y1,z1),(-1.4,y1,z1)],[(0,1,2,3)])
    for k in [-1,1]:
        tube('stone_light',[(k*1.38,y0,z0+.05),(k*1.38,y1,z1+.05)],.09)
    for k in range(3):
        pts=[]
        for j in range(25):
            t=j/24
            pts.append((.60*math.sin(t*math.tau*1.4+k),y0+.2+t*3.0,z0+.1+t*(z1-z0)*.9))
        tube('stone_light',pts,.055,6)

# Hall platform and colonnade.
box('stone_light',(0,0,3.76),(47,25.6,.32))
box('darkred',(0,0,4.02),(44.8,23.8,.20))
# Main enclosed body behind open veranda.
box('red',(0,1,7.0),(40.6,17.5,5.9))
# Eleven bays, twelve front/back columns.
xs=[-22+i*4 for i in range(12)]
for y in [-11.6,11.6]:
    for x in xs:
        cylinder('stone_light',x,y,3.96,.58,.34)
        cylinder('red',x,y,4.28,.34,6.30,16)
        cylinder('gold',x,y,4.34,.355,.12)
        box('teal',(x,y,10.42),(1.05,1.05,.35))
for x in [-22,22]:
    for y in [-7.7,-3.85,0,3.85,7.7]:
        cylinder('stone_light',x,y,3.96,.58,.34)
        cylinder('red',x,y,4.28,.34,6.30,16)
        box('teal',(x,y,10.42),(1.05,1.05,.35))

# Lattice door panels front/back, gilded transoms, dark interior apertures.
for y in [-7.80,9.80]:
    outward=-1 if y<0 else 1
    for x in [-18,-14,-10,-6,-2,2,6,10,14,18]:
        box('darkred',(x,y,6.9),(3.75,.19,5.4))
        for dx in [-1.30,-.43,.43,1.30]:
            xx=x+dx
            box('dark',(xx,y+outward*.12,7.65),(.72,.1,3.45))
            box('red',(xx,y+outward*.19,5.28),(.72,.12,1.25))
            for lx in [-.35,0,.35]:
                box('gold',(xx+lx,y+outward*.22,7.65),(.035,.045,3.4))
            for zz in range(12):
                box('gold',(xx,y+outward*.22,6.05+zz*.29),(.74,.045,.035))
            box('gold',(xx,y+outward*.26,5.3),(.5,.045,.035))
            sphere('gold',(xx+.20,y+outward*.29,5.75),.065)
        for z in [4.26,5.95,9.45]:
            box('red',(x,y+outward*.25,z),(3.84,.24,.16))
# Side wall window lattices.
for x in [-20.35,20.35]:
    sign=1 if x>0 else -1
    for y in [-5,-1,3,7]:
        box('dark',(x+sign*.04,y,7.4),(.12,3.35,3.7))
        for i in range(9):
            box('gold',(x+sign*.13,y-1.6+i*.4,7.4),(.05,.035,3.7))
        for i in range(11):
            box('gold',(x+sign*.13,y,5.6+i*.35),(.05,3.3,.035))

# Painted beam bands and rhythmic dougong bracket clusters.
for z,width,height,key in [(9.9,24,.42,'teal'),(10.28,24.5,.16,'gold'),(10.55,25.2,.35,'blue'),(10.86,26,.18,'teal')]:
    box(key,(0,0,z),(45.8,width,height))
# Mask above is solid cornice, details on front and rear outer faces.
for y in [-12.25,12.25]:
    for x in range(-20,21,4):
        box('blue',(x,y,9.92),(3.3,.1,.35))
        box('gold',(x,y*1.008,9.92),(2.7,.04,.035))
        # Diamond motif.
        meshpart('gold',[(x-.32,y*1.012,9.92),(x,y*1.012,10.07),(x+.32,y*1.012,9.92),(x,y*1.012,9.77)],[(0,1,2,3)])
for x,y in [(x,y) for y in [-11.8,11.8] for x in range(-22,23,2)]+[(x,y) for x in [-22,22] for y in range(-10,11,2)]:
    for j in range(3):
        box('teal',(x,y,10.65+j*.23),(.56+j*.34,.66+j*.38,.17))
        box('gold',(x,y,10.74+j*.23),(.60+j*.34,.70+j*.38,.035))

# Upper clerestory between double eaves.
box('red',(0,0,14.55),(41.5,19.8,2.1))
for y in [-10.02,10.02]:
    for x in range(-20,21,2):
        box('teal',(x,y,15.12),(1.6,.16,.65))
        box('gold',(x,y*1.006,15.12),(1.30,.05,.045))
        box('red',(x+.9,y,14.95),(.18,.32,1.3))
box('blue',(0,0,15.56),(43.2,21.5,.30))

# Curved hip roofs. Four tiled patches share hip edges and terminate at ridge.
def roof(a,b,eave,height,ridge,label):
    def point(side,u,t):
        s=2*u-1
        if side==0: x,y=a*s,-b; rx,ry=ridge*s,0
        elif side==1: x,y=a,b*s; rx,ry=ridge,0
        elif side==2: x,y=-a*s,b; rx,ry=-ridge*s,0
        else: x,y=-a,-b*s; rx,ry=-ridge,0
        z=eave+height*(.55*t+.45*t*t)+(.25+.90*abs(s)**8)*(1-t)**4
        return (x*(1-t)+rx*t,y*(1-t)+ry*t,z)
    for side in range(4):
        n=round((2*a if side%2==0 else 2*b)/.48)
        rows=24
        for i in range(n):
            for j in range(rows):
                t0=j/rows;t1=(j+1)/rows
                verts=[point(side,i/n,t0),point(side,(i+1)/n,t0),point(side,(i+1)/n,t1),point(side,i/n,t1)]
                meshpart('tile'+str(random.randrange(6)),verts,[(0,1,2,3)])
        # Raised rolled tile ribs follow curved slope.
        for i in range(n+1):
            pts=[tuple(Vector(point(side,i/n,j/24))+Vector((0,0,.055))) for j in range(25)]
            tube('tile4',pts,.072,6)
            p=point(side,i/n,0)
            sphere('gold',(p[0],p[1],p[2]-.035),.095)
        for dz,key,r in [(0,'tile3',.16),(-.23,'gold',.09),(-.40,'darkred',.15),(-.57,'teal',.12)]:
            pts=[tuple(Vector(point(side,i/80,0))+Vector((0,0,dz))) for i in range(81)]
            tube(key,pts,r)
        # Hip ridge and small marching beast silhouettes.
        pts=[tuple(Vector(point(side,0,j/40))+Vector((0,0,.15))) for j in range(41)]
        tube('gold',pts,.18)
        for j in range(6):
            t=.07+j*.052
            p=Vector(point(side,0,t))+Vector((0,0,.38))
            sphere('gold',tuple(p),.15)
            tube('gold',[tuple(p-Vector((0,0,.10))),tuple(p+Vector((0,0,.27)))],.08,8)
            sphere('gold',tuple(p+Vector((0,0,.30))),.105)
    z=eave+height
    tube('gold',[(-ridge,0,z+.16),(ridge,0,z+.16)],.23,12)
    for sign in [-1,1]:
        # Stylized chiwen finials curling upward at ridge ends.
        pts=[(sign*(ridge-.5),0,z+.2),(sign*ridge,0,z+.5),(sign*(ridge+.35),0,z+1.1),(sign*(ridge+.15),0,z+1.65),(sign*(ridge-.35),0,z+1.85)]
        tube('gold',pts,.25,10,end_radius=.12)
        sphere('gold',(sign*(ridge+.05),0,z+1.3),.32)
roof(26.5,15.1,11.45,4.65,10.8,'Lower')
roof(24.0,13.3,16.0,6.0,10.5,'Upper')

# Front plaque hangs below first eave. Gold calligraphy if Windows font exists.
box('gold',(0,-12.4,9.44),(4.0,.25,1.15))
box('blue',(0,-12.56,9.44),(3.76,.09,.95))
fontpath=Path('C:/Windows/Fonts/msyh.ttc')
if fontpath.exists():
    font=bpy.data.fonts.load(str(fontpath))
    curve=bpy.data.curves.new('Taihe Hall plaque','FONT')
    curve.body='太 和 殿'
    curve.font=font
    curve.align_x='CENTER';curve.align_y='CENTER';curve.size=.67
    curve.extrude=.008
    obj=bpy.data.objects.new('太和殿 · Hall of Supreme Harmony',curve)
    bpy.context.collection.objects.link(obj)
    obj.location=(0,-12.63,9.44);obj.rotation_euler=(math.pi/2,0,0)
    curve.materials.append(M['gold'])

# Bronze ceremonial incense burners and large fire vats on upper terrace.
for x in [-24,24]:
    for y in [-11,9]:
        cylinder('stone_light',x,y,3.6,.94,.25)
        for dx,dy in [(-.45,-.35),(.45,-.35),(0,.5)]:
            cylinder('bronze',x+dx,y+dy,3.85,.12,.65)
        tube('bronze',[(x,y,4.28),(x,y,4.85)],.70,16,end_radius=.96)
        cylinder('gold',x,y,4.84,.98,.10,20)
        tube('bronze',[(x,y,4.94),(x,y,5.34)],1.01,16,end_radius=.22)
        sphere('gold',(x,y,5.46),.17)
        for s in [-1,1]:
            tube('bronze',[(x+s*.82,y,4.55),(x+s*1.23,y,4.75),(x+s*1.23,y,5.08),(x+s*.88,y,5.05)],.09)
# Pair of gilded courtyard vats and carved marble pedestal accents.
for x in [-17,17]:
    y=-14.2
    cylinder('stone_light',x,y,3.6,.85,.25)
    tube('bronze',[(x,y,3.85),(x,y,4.12),(x,y,4.8)],.65,18,end_radius=.94)
    cylinder('gold',x,y,4.78,.97,.10,20)
    cylinder('dark',x,y,4.82,.83,.025,20)

# Finalize combined meshes organized by architectural material.
collection=bpy.data.collections.new('Taihe Hall | procedural architecture')
bpy.context.scene.collection.children.link(collection)
for key,(verts,faces) in B.items():
    mesh=bpy.data.meshes.new(key+' geometry')
    mesh.from_pydata(verts,[],faces);mesh.update()
    obj=bpy.data.objects.new(key,mesh)
    collection.objects.link(obj)
    obj.data.materials.append(M[key])
# Explicit metadata, no claim of metric accuracy.
scene=bpy.context.scene
scene['asset_description']='Taihe Hall, Forbidden City; interpretive presentation model, not measured reconstruction.'
scene['features']='Double-eaved curved hip roof; 11-bay colonnade; three marble terraces; imperial ramp; ceremonial bronze; courtyard.'
scene.unit_settings.system='METRIC'
world=bpy.data.worlds.new('Warm clear sky')
scene.world=world;world.use_nodes=True
nodes=world.node_tree.nodes;nodes.clear()
out=nodes.new('ShaderNodeOutputWorld');bg=nodes.new('ShaderNodeBackground')
bg.inputs['Color'].default_value=(.52,.67,.85,1);bg.inputs['Strength'].default_value=.45
world.node_tree.links.new(bg.outputs[0],out.inputs[0])
light=bpy.data.lights.new('Late morning sun','SUN');light.energy=2.5;light.angle=math.radians(12)
obj=bpy.data.objects.new('Late morning sun',light);scene.collection.objects.link(obj)
obj.rotation_euler=(math.radians(28),math.radians(-25),math.radians(-35))
light=bpy.data.lights.new('Soft front fill','AREA');light.energy=1800;light.shape='DISK';light.size=40
obj=bpy.data.objects.new('Soft front fill',light);scene.collection.objects.link(obj);obj.location=(0,-35,35)
obj.rotation_euler=(Vector((0,0,8))-obj.location).to_track_quat('-Z','Y').to_euler()
camdata=bpy.data.cameras.new('Presentation camera');cam=bpy.data.objects.new('Presentation camera',camdata)
scene.collection.objects.link(cam);cam.location=(78,-112,71)
target=Vector((0,-1,7.2));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
camdata.type='ORTHO';camdata.ortho_scale=108;camdata.lens=45;camdata.clip_end=1000;scene.camera=cam
# Prefer EEVEE; retain Cycles fallback for builds without it.
scene.render.engine='CYCLES'
for engine in ['BLENDER_EEVEE_NEXT','BLENDER_EEVEE']:
    try:
        scene.render.engine=engine
        break
    except TypeError:
        pass
scene.render.resolution_x=1600;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.render.filepath=str(ROOT/'taihe_preview.png')
scene.render.film_transparent=False
scene.view_settings.view_transform='AgX'
# Ready-to-view viewport, independent of render completion.
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            space=area.spaces.active
            space.clip_end=2000
            space.shading.type='MATERIAL'
            space.shading.use_scene_world=False
            space.overlay.show_overlays=False
            region=space.region_3d
            region.view_distance=115
            region.view_location=target
            region.view_rotation=cam.rotation_euler.to_quaternion()
            region.view_perspective='CAMERA'
bpy.ops.object.select_all(action='DESELECT')
# Pack optional font; model needs no external textures.
bpy.ops.file.pack_all()
blend=ROOT/'TaiheHall.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(blend))
bounds_points=[o.matrix_world @ Vector(corner) for o in scene.objects if o.type=='MESH' for corner in o.bound_box]
actual_bounds={axis:[min(p[i] for p in bounds_points),max(p[i] for p in bounds_points)] for i,axis in enumerate('xyz')}
report={'blender':bpy.app.version_string,'engine':scene.render.engine,'objects':len(scene.objects),
        'vertices':sum(len(o.data.vertices) for o in scene.objects if o.type=='MESH'),
        'polygons':sum(len(o.data.polygons) for o in scene.objects if o.type=='MESH'),
        'bounds':actual_bounds,
        'blend':str(blend),'preview':scene.render.filepath,'interpretive_not_survey':True}
(ROOT/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('MODEL_VALIDATION',json.dumps(report),flush=True)
bpy.ops.render.render(write_still=True)
print('RENDER_COMPLETE',scene.render.filepath,flush=True)
