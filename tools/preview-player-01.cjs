// Orthographic rest-mesh inspection, independent of game lighting and camera setup.
const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');
const root = path.join(__dirname,'..');
const source = fs.readFileSync(path.join(root,'DrectX11Sample/assets/model/Player_01/Player_01.fbx'),'utf8');
const details=process.argv.includes('--details');
const width=1600,height=details?800:640;
const pixels=Buffer.alloc(width*height*3,207);
const depth=new Float32Array(width*height).fill(-Infinity);
const unit=a=>a.map(x=>x/Math.hypot(...a));
const light=unit([-.46,.7,.65]);
for(const mesh of source.split(/(?=Geometry: \d+,)/).slice(1)) {
    const values=name=>mesh.match(new RegExp(name+': \\*\\d+ \\{ a: ([^}]+)'))[1].split(',').map(Number);
    const points=values('Vertices'),normals=values('Normals'),indices=values('PolygonVertexIndex');
    const alloy=mesh.startsWith('Geometry:') && mesh.split('\n')[0].includes('Alloy');
    const views=details?[168,136,102,7].flatMap((target,col)=>[0,-.65].map((angle,row)=>{
        const scale=[13,8.7,10,12][col];
        return [400*col+200,angle,scale,row*400+200+target*scale,col*400,row*400];
    })):[[295,0,3.05],[799,-.76,3.05],[1310,Math.PI,3.05]];
    for(const [center,angle,scale,baseline=595,left=0,top=0] of views) {
        const c=Math.cos(angle),s=Math.sin(angle);
        const verts=[];
        for(let i=0;i<points.length;i+=3) {
            const [x,y,z]=points.slice(i,i+3),[nx,ny,nz]=normals.slice(i,i+3);
            verts.push([center+scale*(x*c-z*s),baseline-y*scale,x*s+z*c,nx*c-nz*s,ny,nx*s+nz*c]);
        }
        for(let i=0;i<indices.length;i+=3) {
            const [a,b,d]=indices.slice(i,i+3).map(k=>verts[k<0?-k-1:k]);
            const area=(b[1]-d[1])*(a[0]-d[0])+(d[0]-b[0])*(a[1]-d[1]);
            if(Math.abs(area)<.00001) continue;
            const x0=Math.max(0,Math.floor(Math.min(a[0],b[0],d[0]))),x1=Math.min(width-1,Math.ceil(Math.max(a[0],b[0],d[0])));
            const y0=Math.max(0,Math.floor(Math.min(a[1],b[1],d[1]))),y1=Math.min(height-1,Math.ceil(Math.max(a[1],b[1],d[1])));
            for(let y=Math.max(top,y0);y<=Math.min(y1,details?top+399:y1);y++) for(let x=Math.max(left,x0);x<=Math.min(x1,details?left+399:x1);x++) {
                const u=((b[1]-d[1])*(x+.5-d[0])+(d[0]-b[0])*(y+.5-d[1]))/area;
                const v=((d[1]-a[1])*(x+.5-d[0])+(a[0]-d[0])*(y+.5-d[1]))/area,w=1-u-v;
                if(Math.min(u,v,w)<0) continue;
                const interp=k=>u*a[k]+v*b[k]+w*d[k],z=interp(2),p=y*width+x;
                if(z<=depth[p]) continue;
                depth[p]=z;
                const n=unit([interp(3),interp(4),interp(5)]);
                const dot=n.reduce((t,e,j)=>t+e*light[j],0);
                const intensity=(alloy?190:47)*(.49+.51*Math.max(dot,0))+(alloy?90:8)*Math.pow(Math.max(0,2*n[2]*dot-light[2]),10);
                for(let j=0;j<3;j++) pixels[3*p+j]=Math.max(0,Math.min(255,intensity+j*1.5));
            }
        }
    }
}
function chunk(name,data) {
    const payload=Buffer.concat([Buffer.from(name),data]),header=Buffer.alloc(4),tail=Buffer.alloc(4);
    let crc=-1;
    for(const byte of payload) {
        crc^=byte;
        for(let j=0;j<8;j++) crc=(crc>>>1)^((crc&1)?0xedb88320:0);
    }
    header.writeUInt32BE(data.length);
    tail.writeUInt32BE((~crc)>>>0);
    return Buffer.concat([header,payload,tail]);
}
const ihdr=Buffer.alloc(13);
ihdr.writeUInt32BE(width); ihdr.writeUInt32BE(height,4); ihdr[8]=8; ihdr[9]=2;
const rows=Array.from({length:height},(_,y)=>Buffer.concat([Buffer.alloc(1),pixels.subarray(y*width*3,(y+1)*width*3)]));
fs.writeFileSync(path.join(root,details?'docs/Player_01/ArmourDetails.png':'docs/Player_01/MeshPreview.png'),Buffer.concat([
    Buffer.from('89504e470d0a1a0a','hex'),chunk('IHDR',ihdr),chunk('IDAT',zlib.deflateSync(Buffer.concat(rows))),chunk('IEND',Buffer.alloc(0))
]));
