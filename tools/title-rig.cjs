const fs = require('fs'), z = require('zlib');
const b = fs.readFileSync('DrectX11Sample/assets/model/Debugman/Akai.fbx');
function read(p) {
  const end=b.readUInt32LE(p), count=b.readUInt32LE(p+4), len=b[p+12];
  if(!end)return null;
  const name=b.toString('utf8',p+13,p+13+len), props=[], vals=[], kids=[];
  p+=13+len;
  for(let i=0;i<count;i++){
    const start=p,t=String.fromCharCode(b[p++]);let v;
    if(t==='L'){v=Number(b.readBigInt64LE(p));p+=8}
    else if(t==='D'){v=b.readDoubleLE(p);p+=8}
    else if(t==='I'){v=b.readInt32LE(p);p+=4}
    else if(t==='S'||t==='R'){let n=b.readUInt32LE(p);p+=4;v=b.toString('utf8',p,p+n);p+=n}
    else if('dfilbc'.includes(t)){let n=b.readUInt32LE(p+8);p+=12+n}
    else p+=({F:4,Y:2,C:1})[t];
    props.push(b.subarray(start,p));vals.push(v);
  }
  while(p+13<=end){const child=read(p);if(!child)break;kids.push(child);p=child.end}
  return {name,props,vals,kids,end};
}
let roots=[];for(let p=27;;){let n=read(p);if(!n)break;roots.push(n);p=n.end}
let objs=roots.find(n=>n.name==='Objects'), con=roots.find(n=>n.name==='Connections');
let models=objs.kids.filter(n=>n.name==='Model');
const bones=models.filter(n=>n.vals[2]==='LimbNode');
const boneName=n=>n.vals[1].split('\0')[0].split(':').pop();
function array(n){let p=n.props[0], data=p.subarray(13);if(p.readUInt32LE(5))data=z.inflateSync(data);return Array.from({length:p.readUInt32LE(1)},(_,i)=>data.readDoubleLE(i*8))}
const binds=new Map();
for(const bone of bones){
  const links=con.kids.filter(c=>c.vals[1]===bone.vals[0]);
  const cluster=objs.kids.find(n=>n.vals[2]==='Cluster'&&links.some(c=>c.vals[2]===n.vals[0]));
  if(cluster)binds.set(boneName(bone),cluster);
}
const pos=name=>array(binds.get(name).kids.find(n=>n.name==='TransformLink')).slice(12,15);
const objects=[], links=[], idByName=new Map();let serial=100;
const quote=x=>typeof x==='string'?JSON.stringify(x):String(x);
const connect=(child,parent)=>links.push('C: "OO",'+child+','+parent);
const arr=(name,values)=>name+': *'+values.length+' { a: '+values.join(',')+' }';
const props=n=>'Properties70: {\n'+(n.kids.find(k=>k.name==='Properties70')?.kids||[]).map(k=>'P: '+k.vals.map(quote).join(',')).join('\n')+'\n}';
for(const n of bones){const id=serial++;idByName.set(boneName(n),id);objects.push('Model: '+id+',"Model::'+n.vals[1].split('\0')[0]+'","LimbNode" {\n'+props(n)+'\n}')}
for(const n of bones){let parent=bones.find(p=>con.kids.some(c=>c.vals[1]===n.vals[0]&&c.vals[2]===p.vals[0]));connect(idByName.get(boneName(n)),parent?idByName.get(boneName(parent)):0)}
const mats=['Titanium','Graphite','Visor'];const buffers=mats.map(()=>({v:[],f:[],weights:new Map()}));
// Closed elliptical lofts: each rigid shell follows one joint, leaving clearance at the hinges.
function loft(bone,mat,center,axis,width,depth,length,rings){
 const g=buffers[mat], base=g.v.length/3, count=24, norm=v=>v.map(x=>x/Math.hypot(...v));
 const cross=(a,b)=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
 const u=norm(axis), x=norm(cross(u,Math.abs(u[2])<0.9?[0,0,1]:[0,1,0])), y=cross(u,x);
 if(!g.weights.has(bone))g.weights.set(bone,[]);
 for(const [t,r] of rings){for(let j=0;j<count;j++){const a=j*2*Math.PI/count;g.weights.get(bone).push(g.v.length/3);g.v.push(...center.map((c,i)=>+(c+u[i]*length*t+x[i]*width*r*Math.cos(a)+y[i]*depth*r*Math.sin(a)).toFixed(5)))}}
 for(let k=1;k<rings.length;k++)for(let j=0;j<count;j++){let a=base+(k-1)*count+j,b=base+(k-1)*count+(j+1)%count;g.f.push(a,b,~(b+count),a,b+count,~(a+count))}
 for(let j=1;j<count-1;j++){let top=base+(rings.length-1)*count;g.f.push(base,base+j+1,~(base+j),top,top+j,~(top+j+1))}
}
const shell=[[-.5,.62],[-.46,.83],[-.3,.98],[.12,1],[.36,.9],[.48,.71],[.5,.6]];
const round=[[-.5,.12],[-.45,.44],[-.32,.77],[0,1],[.32,.77],[.45,.44],[.5,.12]];
function body(b,m,offset,w,d,h,profile=shell){loft(b,m,pos(b).map((x,i)=>x+offset[i]),[0,1,0],w,d,h,profile)}
body('Hips',1,[0,-2,0],13,9,21);body('Hips',0,[0,0,3],13,8,17);
body('Spine',1,[0,2,0],10,8,15);body('Spine1',1,[0,0,0],12,9,16);
body('Spine2',0,[0,2,0],17,11,25,[[-.5,.63],[-.38,.77],[0,1],[.32,.98],[.5,.68]]);
body('Neck',1,[0,2,0],5,5,13);body('Head',1,[0,5,0],8,8,23,round);
body('Head',0,[0,6,1],8.1,8.5,22,[[-.5,.43],[-.34,.67],[-.04,.97],[.25,1],[.42,.8],[.5,.42]]);
body('Head',2,[0,4,5.6],6.5,4.3,16,[[-.5,.35],[-.25,.65],[.18,1],[.45,.85],[.5,.6]]);
function limb(b,e,w,d,fill=.8){const a=pos(b),v=pos(e).map((n,i)=>n-a[i]);loft(b,0,a.map((n,i)=>n+.5*v[i]),v,w,d,Math.hypot(...v)*fill,shell);body(b,1,[0,0,0],w*.83,d*.84,w*1.65,round)}
for(const s of ['Left','Right']){
 limb(s+'Arm',s+'ForeArm',5,5.4,.85);body(s+'Arm',0,[0,1,0],7.2,7,13,round);
 limb(s+'ForeArm',s+'Hand',4.9,4.6,.89);limb(s+'Hand',s+'HandMiddle1',2.7,4.2,1);
 for(const finger of ['Thumb','Index','Middle','Ring','Pinky'])for(let i=1;i<=3;i++)limb(s+'Hand'+finger+i,s+'Hand'+finger+(i+1),1.05,.95,.96);
 limb(s+'UpLeg',s+'Leg',8.2,8.8,.91);limb(s+'Leg',s+'Foot',6,6.8,.87);
 body(s+'Leg',0,[0,0,3.4],5.5,4.1,11);
 body(s+'Foot',1,[0,-3,4],5.2,10.2,9);body(s+'Foot',0,[0,-3,5],5,10,8);
 body(s+'ToeBase',0,[0,2,3],5,7,5);
}
for(let m=0;m<mats.length;m++){
 const {v,f,weights}=buffers[m],name=mats[m],model=serial++,geo=serial++,skin=serial++,mat=serial++;
 const rgb=[[.76,.78,.82],[.075,.079,.09],[.022,.027,.036]][m];
 objects.push('Model: '+model+',"Model::'+name+'","Mesh" { Version: 232 }');
 objects.push('Material: '+mat+',"Material::'+name+'","" {\nShadingModel: "phong"\nProperties70: {\nP: "DiffuseColor","Color","","A",'+rgb+'\nP: "SpecularColor","Color","","A",0.7,0.7,0.7\nP: "Shininess","double","Number","",60\n}\n}');
 objects.push('Geometry: '+geo+',"Geometry::'+name+'","Mesh" {\n'+arr('Vertices',v)+'\n'+arr('PolygonVertexIndex',f)+'\nLayerElementMaterial: 0 {\nVersion: 101\nName: ""\nMappingInformationType: "AllSame"\nReferenceInformationType: "IndexToDirect"\nMaterials: *1 { a: 0 }\n}\nLayer: 0 {\nVersion: 100\nLayerElement: {\nType: "LayerElementMaterial"\nTypedIndex: 0\n}\n}\n}');
 objects.push('Deformer: '+skin+',"Deformer::'+name+'","Skin" { Version: 101 }');
 connect(model,0);connect(geo,model);connect(mat,model);connect(skin,geo);
 for(const [bone,indices] of weights){const cluster=binds.get(bone),id=serial++;objects.push('Deformer: '+id+',"SubDeformer::'+bone+'","Cluster" {\n'+[arr('Indexes',indices),arr('Weights',indices.map(()=>1)),...['Transform','TransformLink'].map(k=>arr(k,array(cluster.kids.find(n=>n.name===k))))].join('\n')+'\n}');connect(id,skin);connect(idByName.get(bone),id)}
}
const dir='DrectX11Sample/assets/model/TitleMannequin';
fs.mkdirSync(dir,{recursive:true});
fs.writeFileSync(dir+'/TitleMannequin.fbx','; FBX 7.4.0 project file\nFBXHeaderExtension: {\nFBXHeaderVersion: 1003\nFBXVersion: 7400\n}\nGlobalSettings: {\nVersion: 1000\n'+props(roots.find(n=>n.name==='GlobalSettings'))+'\n}\nObjects: {\n'+objects.join('\n')+'\n}\nConnections: {\n'+links.join('\n')+'\n}\n');
console.log(bones.length+' bones; '+buffers.reduce((n,g)=>n+g.f.length/3,0)+' triangles');
