import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { createRequire } from 'node:module';

const root=process.cwd();
const dir=path.join(root,'docs/Customize');
const walk=d=>fs.readdirSync(d,{withFileTypes:true}).flatMap(e=>e.isDirectory()?walk(path.join(d,e.name)):[path.join(d,e.name)]);
const pages=walk(dir).filter(p=>p.endsWith('.html'));
const errors=[];
const ids=new Map(pages.map(p=>[p,new Set([...fs.readFileSync(p,'utf8').matchAll(/\bid="([^"]+)"/g)].map(m=>m[1]))]));
let linkCount=0;
for(const file of pages){
 const html=fs.readFileSync(file,'utf8');
 if(html.includes('\ufffd'))errors.push('Replacement character: '+file);
 for(const m of html.matchAll(/(?:href|src)="([^"]+)"/g)){
  if(/^(https?:|data:)/.test(m[1]))continue;
  const [target,hash]=m[1].split('#');
  const dest=path.resolve(path.dirname(file),target||path.basename(file));
  linkCount++;
  if(!fs.existsSync(dest))errors.push('Missing target '+file+' -> '+m[1]);
  else if(hash&&!ids.get(dest)?.has(hash))errors.push('Missing anchor '+file+' -> '+m[1]);
 }
}
const inventory=JSON.parse(fs.readFileSync(path.join(dir,'inventory.json'),'utf8'));
const unexplained=inventory.files.flatMap(f=>f.functions.filter(fn=>!fn.documented&&!fn.individualExplanation).map(fn=>f.path+'::'+fn.name));
const result={pages:pages.length,linksChecked:linkCount,linkErrors:errors,withoutContractOrExplanation:unexplained,browser:null};
if(process.argv.includes('--browser')){
 const moduleRoot=process.env.BOOK_NODE_MODULES;
 if(!moduleRoot)throw Error('Set BOOK_NODE_MODULES to bundled node_modules.');
 const require=createRequire(import.meta.url);
 const {chromium}=require(path.join(moduleRoot,'playwright'));
 const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
 const page=await browser.newPage({viewport:{width:1440,height:1000},deviceScaleFactor:1});
 const consoleErrors=[];page.on('pageerror',e=>consoleErrors.push(e.message));
 const checks=[];
 fs.mkdirSync(path.join(dir,'qa'),{recursive:true});
 try{
  for(const name of ['index.html','chapter-6.html','chapter-7.html','files/Controller--CustomizeMotionEditorController.cpp.html']){
   await page.goto(pathToFileURL(path.join(dir,name)).href);
   checks.push({page:name,h1:await page.locator('h1').textContent(),bodyOverflow:await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth)});
   await page.screenshot({path:path.join(dir,'qa',name.replaceAll('/','--')+'.png')});
  }
  await page.goto(pathToFileURL(path.join(dir,'atlas.html')).href);
  await page.locator('#search').fill('ApplyPosePreset');
  const filtered=await page.locator('[data-search]:visible').count();
  if(filtered<1)errors.push('Search did not return ApplyPosePreset.');
  await page.setViewportSize({width:800,height:1000});
  await page.goto(pathToFileURL(path.join(dir,'chapter-7.html')).href);
  checks.push({page:'chapter-7 at 800px',bodyOverflow:await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth)});
  await page.screenshot({path:path.join(dir,'qa','chapter-7-narrow.png')});
  result.browser={checks,searchVisibleFiles:filtered,consoleErrors};
  if(checks.some(c=>c.bodyOverflow))errors.push('Page overflow detected.');
  errors.push(...consoleErrors);
 }finally{await browser.close();}
}
fs.writeFileSync(path.join(dir,'verification.json'),JSON.stringify(result,null,2));
console.log(JSON.stringify(result,null,2));
if(errors.length||unexplained.length)process.exitCode=1;
