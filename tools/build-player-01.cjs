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
const useSurfaceAtlas = process.argv.includes('--surface-atlas');
const groups = ['Alloy','Flex','Trim'].map(name => ({name,v:[],n:[],uv:[],f:[],bones:new Map(), islands:[]}));
// Each section gives normalized length, two independent radii and optional center offsets.
function loft(material,bone,a,b,sections,count=24,exponent=1) {
    const g=groups[material], axis=sub(b,a), direction=unit(axis);
    const u=unit(cross(Math.abs(direction[2])>.85?[0,1,0]:[0,0,1],direction)), v=cross(direction,u);
    const base=g.v.length;
    g.islands.push({ bone, start: base, wrap: true });
    if (!g.bones.has(bone)) g.bones.set(bone,[]);
    for (const [t,w,d,du=0,dv=0,rearOffset=0] of sections) {
        for (let j=0;j<count;j++) {
            const s=2*Math.PI*j/count, profile=x=>Math.sign(x)*Math.abs(x)**exponent;
            g.bones.get(bone).push(g.v.length);
            const depth = dv + d * profile(Math.sin(s)) + rearOffset * Math.max(0, -Math.sin(s)) ** 2;
            g.v.push(a.map((x,i)=>x+t*axis[i]+u[i]*(du+w*profile(Math.cos(s)))+v[i]*depth));
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
        g.islands.push({ bone, start, wrap: false });
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
body(1,'Hips',91,112,[[0,4,4],[.2,8,6],[.55,11,8.6,0,-.5],[.9,10,8],[1,9,7]]);
// A separate vertex grid for each rim keeps the rim normals out of the broad outer face.
function armour(bone, rows, cols, point, inside, material=0) {
    const g=groups[material];
    if(!g.bones.has(bone)) {
        g.bones.set(bone,[]);
    }
    function grid(n,m,sample,reverse) {
        const start=g.v.length;
        g.islands.push({ bone, start, wrap: false });
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
// Sample adjoining belt plates on one envelope, leaving narrow open seams.
for(const [start,end] of [[-.72,.72],[.745,2.48],[2.505,2*Math.PI-2.505],[-2.48,-.745]]) {
    polarPlate('Hips',6,14,(t,u)=>{
        const a=start+(end-start)*u;
        const front=Math.max(0,Math.cos(a));
        const lower=99.2-8.6*front**5-4.2*Math.max(0,-Math.cos(a))**3;
        const upper=106.4+2.4*Math.sin(a)**2;
        return [a,lower+(upper-lower)*t,13+1.35*t,11.6+.85*t,0,-.4];
    },1.05);
}
for(let i=0;i<3;i++) body(1,i===0?'Hips':'Spine2',105+i*5,114+i*5,[[0,10+i,7.5],[.22,12+i,9],[.7,12.5+i,9.4],[1,11+i,8]]);
// Central abdominal scales are narrower than the surrounding flexible side bands.
for(let i=0;i<3;i++) {
    vertical(2,i===0?'Hips':'Spine2',0,7.4,106+5*i,111.6+5*i,
        [[0,4.3+i*.65,1.6],[.15,5.1+i*.8,2.1],[.8,5.6+i*.8,2.6],[1,5.2+i*.8,2.25]],24,.68);
}
body(1,'Spine2',119,149,[[0,9,6],[.12,11,8,0,-.3],[.36,14,9.7,0,-.9],[.63,16,10.5,0,-.1],[.83,14,8.7,0,-1.1],[1,7,5]],.96);
for(const side of [-1,1]) {
    polarPlate('Spine2',8,12,(t,u)=>{
        const s=side>0?u:1-u;
        const a=side*(.0009+1.59*s);
        const lower=125-5*Math.min(1,Math.max(0,(s-.28)/.49))+2*Math.max(0,(s-.78)/.22);
        const y=lower+(148-2*s-lower)*t;
        const bulge=Math.sin(Math.PI*t);
        // A sloping lower plane meets the rounded upper chest at an oblique ridge.
        // Fade towards the arm socket rather than inflating the entire rib cage.
        const ridge=.43+.13*s;
        const lowerPlane=t/ridge;
        const upperArc=Math.cos((t-ridge)/(1-ridge)*Math.PI/2);
        const profile=t<ridge?lowerPlane:upperArc;
        const depth=10+4.3*bulge-.7*t+(1-s*s)*(5.85*profile-4.3*bulge);
        return [a,y,14+5.3*bulge+1.4*t,depth,0,-.3+.85*(1-s)**4];
    },1.3);
    polarPlate('Spine2',10,16,(t,u)=>{
        const s=side>0?u:1-u;
        const a=Math.PI+side*1.54*s;
        const lower=125-4.6*Math.min(1,Math.max(0,(s-.32)/.49));
        const upper=149-3*s;
        const arch=Math.sin(Math.PI*t);
        const lat=2.15*s*s*4*t*(1-t)*Math.min(1,t/.6);
        return [a,lower+(upper-lower)*t,14+5.3*arch+1.4*t+lat,10.4+4.1*arch-2*t,0,-.8+.6*s];
    },1.25);
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
const helmetCore=headSections.map(([t,w,f,r])=>[t,w*.67,(f-r)*.32,0,(f+r)/2]);
vertical(1,'Head',0,0,156,177.5,helmetCore,40,.9);

// Sagittal stations run from chin to nape. Each row specifies the centreline
// (Y,Z) and the lateral lip (X,Y,Z). The two creases cross the crown, not the opening.
const shellStations=[
    [155.7,7.7,3.35,156.1,5.0],
    [156.8,8.65,4.1,157.1,5.1],
    [159.1,9.9,4.9,159.0,4.9],
    [162.0,10.6,5.8,161.6,4.5],
    [165.0,11.3,6.5,164.2,3.9],
    [168.1,11.72,7.2,166.8,2.8],
    [170.5,11.9,7.8,168.7,1.3],
    [173.0,11.6,8.35,170.1,-.6],
    [175.8,10.7,8.7,171.0,-2.3],
    [178.0,8.4,8.9,171.55,-3.8],
    [179.4,5.8,9.05,171.85,-5.0],
    [180,2,9.1,172.0,-6.0],
    [179.5,-2.6,8.9,172.0,-7.0],
    [178.3,-6.55,8.45,171.7,-7.9],
    [176.6,-8.9,7.9,171.2,-8.5],
    [174.2,-10.12,7.15,170.3,-8.9],
    [172.45,-10.4,6.6,169.4,-8.9],
    [169.0,-9.95,6.2,167.0,-8.65],
    [165.0,-8.45,5.65,164.4,-7.35],
    [162.0,-6.7,4.8,162.0,-5.7]
];
const bands=[-1,-.64,.64,1];
for(let band=1;band<bands.length;band++) {
    const surface=(v,u,inset)=>{
        const row=shellStations[Math.round(v*(shellStations.length-1))];
        const [y,z,w,lipY,lipZ]=row;
        const s=bands[band-1]+(bands[band]-bands[band-1])*u;
        const q=Math.abs(s);
        const bend=.84*(1-Math.sqrt(Math.max(0,1-q*q)))+.16*Math.max(0,(q-.64)/.36);
        const shrink=inset?.94:1;
        return [w*s*shrink,168+(y+(lipY-y)*bend-168)*shrink,
            1+(z+(lipZ-z)*bend-1)*shrink];
    };
    armour('Head',shellStations.length-1,band===2?16:8,
        (v,u)=>surface(v,u,false),(v,u)=>surface(v,u,true));
}
// Horizontal cuts through the opening give both the liner and the guard the
// actual front and rear lip positions, including the sloping jawline.
const cheekContour=shellStations.map(([, ,x,y,z])=>[x,y,z]);
function cheekSection(y) {
    const hits=[];
    for(let i=0;i<cheekContour.length;i++) {
        const a=cheekContour[i],b=cheekContour[(i+1)%cheekContour.length];
        if(y<Math.min(a[1],b[1])||y>=Math.max(a[1],b[1])) {
            continue;
        }
        const t=(y-a[1])/(b[1]-a[1]);
        hits.push([a[0]+(b[0]-a[0])*t,a[2]+(b[2]-a[2])*t]);
    }
    hits.sort((a,b)=>b[1]-a[1]);
    return [hits[0],hits.at(-1)];
}
function cheekPoint(side,y,s,offset) {
    const [front,rear]=cheekSection(y);
    const t=Math.max(0,Math.min(1,s));
    const fullness=Math.min(1,(y-156.1)/12);
    const arch=4*t*(1-t);
    const rim=front[0]+(rear[0]-front[0])*t-.22;
    const x=rim+(8.65-rim)*arch*fullness;
    return [side*Math.min(9.09,x+offset),y,front[1]+(rear[1]-front[1])*s];
}
for(const side of [-1,1]) {
    const cheek=(v,u,offset)=>{
        const y=156.11+15.88*v;
        return cheekPoint(side,y,side>0?u:1-u,offset);
    };
    armour('Head',24,14,(v,u)=>cheek(v,u,0),(v,u)=>cheek(v,u,-.45),1);

    const guard=(v,u,inset)=>{
        const s=side>0?u:1-u;
        const bottom=158.15+4.15*s;
        const top=160.0+11.3*Math.sin(s*Math.PI*.50)**.65;
        const y=bottom+(top-bottom)*v;
        // Roll the forward tip into the cheek; slide the rear edge under the shell.
        const reach=.10+.98*s;
        const thickness=.12+.36*Math.sin(Math.PI*s);
        const tuck=1.15*Math.max(0,(s-.82)/.18);
        return cheekPoint(side,y,reach,thickness-tuck-(inset?.56:0));
    };
    armour('Head',10,20,(v,u)=>guard(v,u,false),(v,u)=>guard(v,u,true));
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
    // Intermediate rings follow the original front contour; only the rear arc bulges.
    const shin=limb(0,'Leg','Foot',[
        [.06,4.9,5],[.16,6.1,7.2,0,-.5,.55],
        [.245,6.85,8.375,0,-1.725,1.35],[.2875,7.225,8.9625,0,-2.3375,1.9],
        [.33,7.6,9.55,0,-2.95,2.6],[.3675,7.375,9.2625,0,-2.8375,2.0],
        [.405,7.15,8.975,0,-2.725,1.65],[.4425,6.925,8.6875,0,-2.6125,1.45],
        [.48,6.7,8.4,0,-2.5,1.35],[.72,4.6,5.5,0,-.3],[.94,3.7,4],[1,3.4,3.6]
    ]);
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
const atlasLayout = {};

// FBX polygon-corner UVs allow a wrapped island to split without changing the mesh.
function packSurfaceAtlas(group) {
    const columns = Math.ceil(Math.sqrt(group.islands.length));
    const padding = 0.04;
    const islandByVertex = [];

    atlasLayout[group.name] = group.islands.map(({ bone, start, wrap }, index) => {
        const end = group.islands[index + 1]?.start ?? group.v.length;
        for (let vertex = start; vertex < end; vertex++) {
            islandByVertex[vertex] = index;
        }

        return {
            bone, wrap,
            u: (index % columns + padding) / columns,
            v: (Math.floor(index / columns) + padding) / columns,
            size: (1 - 2 * padding) / columns
        };
    });

    const packed = [];
    for (let corner = 0; corner < group.f.length; corner += 3) {
        const vertices = [group.f[corner], group.f[corner + 1], -group.f[corner + 2] - 1];
        const island = atlasLayout[group.name][islandByVertex[vertices[0]]];
        const uValues = vertices.map(vertex => group.uv[vertex][0]);
        const crossesSeam = island.wrap && Math.max(...uValues) - Math.min(...uValues) > 0.5;

        for (const vertex of vertices) {
            const [u, v] = group.uv[vertex];
            const localU = crossesSeam && u === 0 ? 1 : u;
            packed.push(island.u + localU * island.size, island.v + v * island.size);
        }
    }

    return packed;
}

for(const g of groups.filter(g=>g.v.length)) {
    const model=id++,geo=id++,mat=id++,skin=id++,tex=id++,video=id++;
    const file = useSurfaceAtlas ? g.name + 'Atlas.color.png' : g.name + '.bmp';
    const rgb = g.name === 'Alloy' ? [192,195,201] : g.name === 'Trim' ? [63,65,69] : [43,45,50];
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
    if (!useSurfaceAtlas) {
        fs.writeFileSync(path.join(out,file),bmp);
    }
    objects+='Model: '+model+',"Model::'+g.name+'","Mesh" { Version: 232 }\n';
    objects+='Geometry: '+geo+',"Geometry::'+g.name+'","Mesh" {\n'+array('Vertices',g.v.flat())+'\n'+array('PolygonVertexIndex',g.f)+'\n';
    const uvData = useSurfaceAtlas ? packSurfaceAtlas(g) : g.uv.flat();
    for(const [type,attr,data] of [['Normal','Normals',g.n.map(unit).flat()],['UV','UV',uvData]]) {
        const mapping = useSurfaceAtlas && type === 'UV' ? 'ByPolygonVertex' : 'ByVertice';
        objects+='LayerElement'+type+': 0 {\nVersion: 101\nName: ""\nMappingInformationType: "'+mapping+'"\nReferenceInformationType: "Direct"\n'+array(attr,data)+'\n}\n';
    }
    objects+='LayerElementMaterial: 0 {\nVersion: 101\nName: ""\nMappingInformationType: "AllSame"\nReferenceInformationType: "IndexToDirect"\nMaterials: *1 { a: 0 }\n}\nLayer: 0 {\nVersion: 100\n';
    for(const type of ['Normal','Material','UV']) objects+='LayerElement: {\nType: "LayerElement'+type+'"\nTypedIndex: 0\n}\n';
    const materialName = g.name === 'Alloy'
        ? (useSurfaceAtlas ? 'SatinMetal_AlloyAtlas' : 'SatinMetal_Alloy')
        : g.name;
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
const outputName = useSurfaceAtlas ? 'Player_01_SurfaceAtlas' : 'Player_01';
fs.writeFileSync(path.join(out,outputName+'.fbx'),objects+'}\nConnections: {\n'+connections.join('\n')+'\n}\n');
if (useSurfaceAtlas) {
    fs.writeFileSync(path.join(out, outputName + '.json'), JSON.stringify(atlasLayout, null, 2));
}
console.log({bones:names.size,triangles:total});
