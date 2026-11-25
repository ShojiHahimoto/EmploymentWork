// Generate hard shells in rest space; retain the authored Mixamo bind transforms.
const fs = require('node:fs');
const path = require('node:path');
const root = path.join(__dirname, '../DrectX11Sample/assets/model');
const out = path.join(root, 'Player_01');
fs.mkdirSync(out, { recursive: true });
const source = fs.readFileSync(path.join(root, 'TitleMannequin/TitleMannequin.fbx'), 'utf8');
const header = source.slice(0, source.indexOf('Model: 167,'));
const names = new Map([...header.matchAll(/Model: (\d+),"Model::mixamorig:([^"]+)"/g)].map(m => [m[2], Number(m[1])]));
const bind = new Map();
for (const m of source.matchAll(/"SubDeformer::([^"]+)","Cluster" \{[\s\S]*?(Transform: [^\n]+)\s+(TransformLink: [^\n]+)/g)) {
    bind.set(m[1], { matrix: m[2] + '\n' + m[3], pos: m[3].split('a:')[1].split(',').slice(12,15).map(Number) });
}
const sub = (a,b) => a.map((v,i) => v-b[i]);
const cross = (a,b) => [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]];
const unit = a => a.map(x => x/(Math.hypot(...a)||1));
const groups = ['Alloy','Flex','Trim'].map(name => ({name,v:[],n:[],uv:[],f:[],bones:new Map()}));
// Each section gives normalized length, two independent radii and optional center offsets.
function loft(material,bone,a,b,sections,count=24,exponent=1) {
    const g=groups[material], axis=sub(b,a), direction=unit(axis);
    const u=unit(cross(Math.abs(direction[2])>.85?[0,1,0]:[0,0,1],direction)), v=cross(direction,u);
    const base=g.v.length;
    if (!g.bones.has(bone)) g.bones.set(bone,[]);
    for (const [t,w,d,du=0,dv=0] of sections) {
        for (let j=0;j<count;j++) {
            const s=2*Math.PI*j/count, profile=x=>Math.sign(x)*Math.abs(x)**exponent;
            g.bones.get(bone).push(g.v.length);
            g.v.push(a.map((x,i)=>x+t*axis[i]+u[i]*(du+w*profile(Math.cos(s)))+v[i]*(dv+d*profile(Math.sin(s)))));
            g.n.push([0,0,0]);
            g.uv.push([j/count,t]);
        }
    }
    function tri(i,j,k) {
        g.f.push(i,j,-k-1);
        const n=cross(sub(g.v[j],g.v[i]),sub(g.v[k],g.v[i]));
        for(const p of [i,j,k]) g.n[p]=g.n[p].map((x,c)=>x+n[c]);
    }
    for(let r=0;r<sections.length-1;r++) for(let j=0;j<count;j++) {
        const i=base+r*count+j,k=base+r*count+(j+1)%count;
        tri(i,k,i+count);
        tri(k,k+count,i+count);
    }
    const end=base+(sections.length-1)*count;
    for(let j=1;j<count-1;j++) {
        tri(base,base+j+1,base+j);
        tri(end,end+j,end+j+1);
    }
    return {axis,a,u,v,sections,exponent,bone};
}
// A shallow bevel and an exposed graphite lip follow the same section as the shell.
// Angles are measured from the forward-facing meridian toward the sides.
function plate(shell, outline, angle=0, lift=.45, ridge=.24) {
    const {a,axis,u,v,sections,exponent,bone}=shell;
    for(const layer of [1,0]) {
        const g=groups[layer],start=g.v.length,steps=16;
        if(!g.bones.has(bone)) g.bones.set(bone,[]);
        for(let row=0;row<outline.length;row++) {
            const [height,half,sweep=0,bend=0]=outline[row];
            for(let j=0;j<=steps;j++) {
                const s=2*j/steps-1;
                const t=Math.max(sections[0][0],Math.min(sections.at(-1)[0],height+bend*s*s));
                const k=Math.max(1,sections.findIndex(p=>p[0]>=t));
                const l=sections[k-1],r=sections[k],f=(t-l[0])/(r[0]-l[0]);
                const mix=i=>(l[i]||0)*(1-f)+(r[i]||0)*f;
                const theta=angle+sweep+s*(half+(layer?.045:0));
                const edge=Math.min(1,j,steps-j,row,outline.length-1-row);
                const h=layer?.16:lift*(.85+.45*edge)+ridge*(1-Math.abs(s))*edge;
                const shape=x=>Math.sign(x)*Math.abs(x)**exponent;
                const x=mix(3)+(mix(1)+h)*shape(Math.sin(theta));
                const z=mix(4)+(mix(2)+h)*shape(Math.cos(theta));
                g.bones.get(bone).push(g.v.length);
                g.v.push(a.map((p,i)=>p+axis[i]*t+u[i]*x+v[i]*z));
                g.uv.push([j/steps,t]);
                g.n.push([0,0,0]);
            }
        }
        for(let row=1;row<outline.length;row++) for(let j=0;j<steps;j++) {
            const i=start+(row-1)*(steps+1)+j,k=i+steps+1;
            for(const [p,q,r] of [[i,k,i+1],[i+1,k,k+1]]) {
                g.f.push(p,q,-r-1);
                const n=cross(sub(g.v[q],g.v[p]),sub(g.v[r],g.v[p]));
                for(const id of [p,q,r]) g.n[id]=g.n[id].map((x,c)=>x+n[c]);
            }
        }
    }
}
function vertical(m,b,x,z,lo,hi,s,n=32,e=1) {
    return loft(m,b,[x,lo,z],[x,hi,z],s,n,e);
}
const body=(m,b,lo,hi,s,e=1)=>vertical(m,b,0,0,lo,hi,s,40,e);
body(1,'Hips',91,112,[[0,5,6],[.2,11,9,0,-1],[.55,14,11.7,0,-1.7],[.9,12,10,0,-1],[1,10,8]]);
// A separate vertex grid for each rim keeps the rim normals out of the broad outer face.
function armour(bone, rows, cols, point, inside, material=0) {
    const g=groups[material];
    if(!g.bones.has(bone)) {
        g.bones.set(bone,[]);
    }
    function grid(n,m,sample,reverse) {
        const start=g.v.length;
        for(let i=0;i<=n;i++) for(let j=0;j<=m;j++) {
            g.bones.get(bone).push(g.v.length);
            g.v.push(sample(i/n,j/m));
            g.uv.push([j/m,i/n]);
            g.n.push([0,0,0]);
        }
        for(let i=0;i<n;i++) for(let j=0;j<m;j++) {
            const a=start+i*(m+1)+j,b=a+m+1;
            for(const face of [[a,a+1,b],[a+1,b+1,b]]) {
                const [p,q,r]=reverse?face.reverse():face;
                g.f.push(p,q,-r-1);
                const normal=cross(sub(g.v[q],g.v[p]),sub(g.v[r],g.v[p]));
                for(const k of face) g.n[k]=g.n[k].map((v,c)=>v+normal[c]);
            }
        }
    }
    grid(rows,cols,point,false);
    grid(rows,cols,inside,true);
    const rim=(edge,count,reverse)=>grid(1,count,(t,s)=>{
        const [v,u]=edge(s),a=point(v,u),b=inside(v,u);
        return a.map((x,i)=>x+(b[i]-x)*t);
    },reverse);
    rim(s=>[0,s],cols,true);
    rim(s=>[1,s],cols,false);
    rim(s=>[s,0],rows,false);
    rim(s=>[s,1],rows,true);
}
function polarPlate(bone,rows,cols,sample,thickness) {
    const point=(t,u,offset)=>{
        const [angle,y,x,z,cx=0,cz=0]=sample(t,u);
        return [cx+(x-offset)*Math.sin(angle),y,(z-offset)*Math.cos(angle)+cz];
    };
    armour(bone,rows,cols,(t,u)=>point(t,u,0),(t,u)=>point(t,u,thickness));
}
polarPlate('Hips',5,14,(t,u)=>{
    const s=2*u-1;
    return [s*(.55+.32*t),90+15*t+3*Math.abs(s)*(1-t),6.8+6*t,8.7+3.5*t,0,1];
},1);
for(let i=0;i<3;i++) body(1,i===0?'Hips':'Spine2',105+i*5,114+i*5,[[0,10+i,7.5],[.22,12+i,9],[.7,12.5+i,9.4],[1,11+i,8]]);
// Central abdominal scales are narrower than the surrounding flexible side bands.
for(let i=0;i<3;i++) {
    vertical(2,i===0?'Hips':'Spine2',0,7.4,106+5*i,111.6+5*i,
        [[0,4.3+i*.65,1.6],[.15,5.1+i*.8,2.1],[.8,5.6+i*.8,2.6],[1,5.2+i*.8,2.25]],24,.68);
}
const chest=body(1,'Spine2',119,149,[[0,9,6],[.12,11,8,0,-.3],[.36,14,9.7,0,-.9],[.63,16,10.5,0,-.1],[.83,14,8.7,0,-1.1],[1,7,5]],.96);
for(const side of [-1,1]) {
    polarPlate('Spine2',8,12,(t,u)=>{
        const s=side>0?u:1-u;
        const a=side*(.0009+1.59*s);
        const lower=125-5*Math.min(1,Math.max(0,(s-.28)/.49))+2*Math.max(0,(s-.78)/.22);
        const y=lower+(148-2*s-lower)*t;
        const bulge=Math.sin(Math.PI*t);
        return [a,y,14+5.3*bulge+1.4*t,10+4.3*bulge-.7*t,0,-.3+.85*(1-s)**4];
    },1.3);
    plate(chest,[[.2,.25,0,.11],[.41,.49],[.66,.54],[.85,.46],[.94,.27,0,-.06]],Math.PI+side*.58,.55);
    polarPlate('Hips',4,14,(t,u)=>{
        const s=side>0?u:1-u;
        return [side*(.65+1.85*s),99+7*t+3*Math.sin(s*Math.PI),14.4+.6*t,12-.6*t,0,-1];
    },1.1);
}
body(1,'Head',146,164,[[0,7.4,6.8],[.18,6.6,6],[.42,5.2,5.1],[.75,5,5],[1,6,6]]);
// One continuous shell. Specify the front and rear silhouettes independently.
const headSections = [
    // Height fraction, half width, front Z, rear Z.
    [0, 3.5, 7.7, 0.3],
    [.045, 4.65, 8.65, -1.4],
    [.14, 6.1, 9.9, -4.3],
    [.26, 7.35, 10.6, -6.7],
    [.38, 8.1, 11.3, -8.45],
    [.51, 8.65, 11.7, -9.75],
    [.61, 8.9, 11.9, -10.28],
    [.69, 9.05, 11.72, -10.4],
    [.76, 9.1, 11.4, -10.12],
    [.86, 8.2, 10.1, -8.9],
    [.94, 6.2, 8.4, -6.55],
    [.98, 3.8, 5.7, -3.7],
    [1, 1.4, 4, 0]
];
const helmetCore=headSections.map(([t,w,f,r])=>[t,w*.91,(f-r)*.455,0,(f+r)/2]);
vertical(1,'Head',0,0,155.7,180,helmetCore,40,.9);
vertical(0,'Head',0,2,179.88,180,[[0,1.72,2.32],[1,1.4,2]],32);
function helmetSurface(t) {
    const k=Math.max(1,headSections.findIndex(row=>row[0]>=t));
    const a=headSections[k-1],b=headSections[k],f=(t-a[0])/(b[0]-a[0]);
    return a.map((v,i)=>v+(b[i]-v)*f);
}
for(const face of [true,false]) {
    polarPlate('Head',20,face?16:32,(v,u)=>{
        const s=2*u-1;
        const t=v+(face?0:.055)*(1-v);
        const [,w,front,rear]=helmetSurface(t);
        const edge=.97+.47*t-.13*Math.sin(Math.PI*t);
        const gap=.026+.18*Math.sin(Math.PI*t)**2;
        const angle=face?s*edge:Math.PI+s*(Math.PI-edge-gap);
        const crown=face?.48*Math.sin(Math.PI*t)*(1-Math.abs(s)):0;
        return [angle,155.7+24.3*t,w,(front-rear)/2,0,(front+rear)/2+crown];
    },.87);
}
// Side guards rise towards the back, exposing a curved recess rather than a painted stripe.
for(const side of [-1,1]) {
    polarPlate('Head',7,12,(v,u)=>{
        const s=side>0?u:1-u;
        const low=.17+.20*s,high=.28+.30*Math.sin(s*Math.PI*.65)+.13*s;
        const t=low+(high-low)*v;
        const [,w,f,r]=helmetSurface(t);
        return [side*(1.03+1.32*s),155.7+24.3*t,w+.42,(f-r)/2+.42,0,(f+r)/2];
    },.63);
}
for(const side of ['Left','Right']) {
    const sign=side==='Left'?1:-1;
    const pos=n=>bind.get(side+n).pos;
    function limb(m,b,end,s,n=28) {return loft(m,side+b,pos(b),pos(end),s,n);}
    function joint(b,r) {
        const [x,y,z]=pos(b);
        vertical(1,side+b,x,z,y-r,y+r,[[0,.2,.2],[.12,.65*r,.65*r],[.32,.94*r,.94*r],[.5,r,r],[.7,.92*r,.92*r],[.9,.6*r,.6*r],[1,.2,.2]],24);
    }
    joint('Arm',8);
    limb(0,'Arm','ForeArm',[[.03,5,6],[.12,8.9,9.4],[.24,8,8.7],[.4,6.6,6.5],[.66,6.1,6.3],[.84,5.1,4.9],[.95,4,4]]);
    const [x,y,z]=pos('Arm');
    const cap=vertical(0,side+'Arm',x+sign*3,z,y-3,y+10,[[0,7,7],[.12,9,9.6],[.5,8.3,9.4],[.84,5.6,6.4],[1,2,2]]);
    plate(cap,[[.09,.85,0,.12],[.26,1.08],[.54,1.16],[.77,1.02],[.93,.56]],-sign*1.25);
    joint('ForeArm',4.5);
    limb(0,'ForeArm','Hand',[[.06,4.4,4.6],[.15,6,6],[.32,6.7,6],[.52,6,5],[.85,3.7,3.9],[.96,3.2,3.3]]);
    limb(1,'ForeArm','Hand',[[.9,3.58,3.62],[.915,3.85,3.92],[.96,3.55,3.63],[.975,3.35,3.4]]);
    joint('Hand',3.4);
    const hand=limb(0,'Hand','HandMiddle1',[[0,2.9,3],[.25,3,4],[.64,2.8,4.9],[1,2,4.5]]);
    plate(hand,[[.16,.45],[.32,.71],[.68,.84],[.91,.67]],sign*Math.PI/2,.22);
    for(const finger of ['Thumb','Index','Middle','Ring','Pinky']) for(let i=1;i<=3;i++) {
        const bone='Hand'+finger+i,a=pos(bone),prev=pos('Hand'+finger+Math.max(1,i-1));
        const b=i<3?pos('Hand'+finger+(i+1)):a.map((x,j)=>x+.78*(x-prev[j]));
        const r=(finger==='Pinky'?.76:1.02)*(1-.09*(i-1));
        joint(bone,r);
        loft(0,side+bone,a,b,[[.08,r,r],[.3,1.06*r,r],[.85,.9*r,.92*r],[1,.66*r,.7*r]],12);
    }
    joint('UpLeg',8.8);
    limb(0,'UpLeg','Leg',[[.08,6.5,7],[.17,9,10.2,0,-.4],[.33,10.1,11.9,0,-.3],[.54,9.1,11.1],[.75,7.2,8.4],[.9,5.8,6],[.96,5,5.5]]);
    joint('Leg',5.8);
    const [kx,ky,kz]=pos('Leg');
    const knee=vertical(1,side+'Leg',kx,kz+4,ky-6,ky+6,[[0,2,1],[.22,4,2.7],[.55,5,3.5],[.84,4.5,3],[1,2.4,1.7]],20,.72);
    plate(knee,[[.06,.72,0,.1],[.25,1.22],[.55,1.27],[.78,1.18],[.95,.76,0,-.08]],0,.58,.48);
    const shin=limb(0,'Leg','Foot',[[.06,4.9,5],[.16,6.1,7.2,0,-.5],[.33,7.6,9.55,0,-2.95],[.48,6.7,8.4,0,-2.5],[.72,4.6,5.5,0,-.3],[.94,3.7,4],[1,3.4,3.6]]);
    plate(shin,[[.12,.54,0,-.03],[.31,.9],[.5,.76],[.72,.57],[.91,.37,0,-.08]],0,.35,.52);
    joint('Foot',3.9);
    const [fx,,fz]=pos('Foot');
    vertical(1,side+'Foot',fx,fz+5,0,3,[[0,4.6,10],[.25,5.2,10.6],[1,5,10.3]],28,.65);
    vertical(1,side+'Foot',fx,fz+3,2,11,[[0,4.45,9.3],[.35,4.2,7],[.76,3.6,4,0,-2],[1,3.25,3.5,0,-3]],24,.72);
    function shoeShell(bone,start,end,widthA,widthB,heightA,heightB) {
        const sample=(t,u,inset)=>{
            const theta=Math.PI*u;
            const width=widthA+(widthB-widthA)*t-inset;
            const height=heightA+(heightB-heightA)*t-inset;
            return [fx+width*Math.sign(Math.cos(theta))*Math.abs(Math.cos(theta))**.54,
                2.05+height*Math.max(0,Math.sin(theta))**.53,
                fz+start+(end-start)*t+.8*(1-Math.abs(Math.cos(theta)))];
        };
        armour(side+bone,3,14,(t,u)=>sample(t,u,0),(t,u)=>{
            const p=sample(t,u,.85);
            // Close toe and heel to the sole rather than leaving a tube opening.
            if((bone==='ToeBase' && t===1)||(start<0 && t===0)) {
                p[1]=1.75;
                p[2]+=t===1?-.24:.24;
            }
            return p;
        });
    }
    shoeShell('Foot',-5.8,.9,4.4,4.75,5.1,8.3);
    shoeShell('Foot',.62,6.5,4.75,5.05,8.7,5.65);
    shoeShell('Foot',6.25,10.8,5.05,5.1,6,4.05);
    shoeShell('ToeBase',10.48,16.65,5.05,4.4,4.45,2.15);
}
const array=(name,values)=>name+': *'+values.length+' { a: '+values.map(v=>Number(v.toFixed(6))).join(',')+' }';
let objects=header, connections=source.split('Connections: {')[1].split('\n').filter(l=>{
    const m=l.match(/"OO",(\d+),(\d+)/);
    return m && Number(m[1])<167 && Number(m[2])<167;
});
let id=400,total=0;
const link=(child,parent)=>connections.push('C: "OO",'+child+','+parent);
for(const g of groups.filter(g=>g.v.length)) {
    const model=id++,geo=id++,mat=id++,skin=id++,tex=id++,video=id++;
    const file=g.name+'.bmp',rgb=g.name==='Alloy'?[192,195,201]:g.name==='Trim'?[63,65,69]:[43,45,50];
    const size=128;
    const bmp=Buffer.alloc(54+size*size*3);
    bmp.write('BM'); bmp.writeUInt32LE(bmp.length,2); bmp.writeUInt32LE(54,10); bmp.writeUInt32LE(40,14);
    bmp.writeInt32LE(size,18); bmp.writeInt32LE(size,22); bmp.writeUInt16LE(1,26); bmp.writeUInt16LE(24,28);
    for(let y=0;y<size;y++) for(let x=0;x<size;x++) {
        const brushed=Math.sin(y*2*Math.PI/8)*1.2+Math.cos((x+5*y)*2*Math.PI/32)*.8;
        const woven=Math.cos((x+y)*Math.PI/4)*Math.cos((x-y)*Math.PI/4)*4;
        const detail=g.name==='Alloy'?brushed:woven;
        for(let c=0;c<3;c++) bmp[54+3*(size*y+x)+c]=Math.round(rgb[2-c]+detail);
    }
    fs.writeFileSync(path.join(out,file),bmp);
    objects+='Model: '+model+',"Model::'+g.name+'","Mesh" { Version: 232 }\n';
    objects+='Geometry: '+geo+',"Geometry::'+g.name+'","Mesh" {\n'+array('Vertices',g.v.flat())+'\n'+array('PolygonVertexIndex',g.f)+'\n';
    for(const [type,attr,data] of [['Normal','Normals',g.n.map(unit).flat()],['UV','UV',g.uv.flat()]]) {
        objects+='LayerElement'+type+': 0 {\nVersion: 101\nName: ""\nMappingInformationType: "ByVertice"\nReferenceInformationType: "Direct"\n'+array(attr,data)+'\n}\n';
    }
    objects+='LayerElementMaterial: 0 {\nVersion: 101\nName: ""\nMappingInformationType: "AllSame"\nReferenceInformationType: "IndexToDirect"\nMaterials: *1 { a: 0 }\n}\nLayer: 0 {\nVersion: 100\n';
    for(const type of ['Normal','Material','UV']) objects+='LayerElement: {\nType: "LayerElement'+type+'"\nTypedIndex: 0\n}\n';
    const materialName=g.name==='Alloy'?'SatinMetal_Alloy':g.name;
    objects+='}\n}\nMaterial: '+mat+',"Material::'+materialName+'","" {\nShadingModel: "phong"\nProperties70: {\nP: "DiffuseColor","Color","","A",'+rgb.map(x=>x/255).join(',')+'\n}\n}\n';
    objects+='Video: '+video+',"Video::'+g.name+'","Clip" {\nType: "Clip"\nFilename: "'+file+'"\nRelativeFilename: "'+file+'"\n}\nTexture: '+tex+',"Texture::'+g.name+'","" {\nType: "TextureVideoClip"\nFileName: "'+file+'"\nRelativeFilename: "'+file+'"\n}\n';
    link(video,tex); connections.push('C: "OP",'+tex+','+mat+',"DiffuseColor"');
    objects+='Deformer: '+skin+',"Deformer::'+g.name+'","Skin" { Version: 101 }\n';
    link(model,0); link(geo,model); link(mat,model); link(skin,geo);
    for(const [bone,indices] of g.bones) {
        const cluster=id++;
        objects+='Deformer: '+cluster+',"SubDeformer::'+bone+'","Cluster" {\n'+array('Indexes',indices)+'\n'+array('Weights',indices.map(()=>1))+'\n'+bind.get(bone).matrix+'\n}\n';
        link(cluster,skin); link(names.get(bone),cluster);
    }
    total+=g.f.length/3;
}
fs.writeFileSync(path.join(out,'Player_01.fbx'),objects+'}\nConnections: {\n'+connections.join('\n')+'\n}\n');
console.log({bones:names.size,triangles:total});
