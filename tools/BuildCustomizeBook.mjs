import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { chapters, roles, notes } from './CustomizeBookContent.mjs';

// This is a source reader, not a C++ compiler. Keep the unabridged source beside
// the lexical index so an unsupported construct never disappears from the book.
const root = process.cwd();
const src = path.join(root, 'DrectX11Sample/Source');
const out = path.join(root, 'docs/Customize');
fs.mkdirSync(path.join(out, 'files'), { recursive: true });
const esc = s => String(s).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
const stems = [
 'Scene/CustomizeScene','Controller/CustomizeTypes','Controller/CustomizeFlowController',
 'Controller/CustomizeAttackEditorController','Controller/CustomizeMotionEditorController',
 'Controller/CustomizePreviewController','Controller/CustomizeCharacterEditorController',
 'Controller/CustomizeEditHistory','Controller/KeyframeEditing','UI/CustomizeAttackEditorView',
 'UI/CustomizeMotionEditorControls','UI/CustomizeMenuView','UI/UIRoot','UI/UIInputSystem',
 'UI/UIRenderer','UI/UITheme','Data/AttackData','Data/AttackDataSaver','Data/CharacterData',
 'Data/CharacterDataLoader','Data/CharacterDataSaver','Data/MotionData','Data/MotionDataLoader',
 'Data/MotionDataSaver','Data/MotionSkeletonDefinition','Data/SkeletonPose','Data/PosePreset',
 'Data/JsonValue','Data/BattleSetupData','System/MotionPose','System/MotionIk',
 'System/MotionRotationLimits','Resource/ModelResource','System/Renderer','System/Game',
 'System/Application','Scene/Scene','Scene/SceneManager','World/World','Core/GameObject',
 'Component/Component','Component/TransformComponent','Component/CameraComponent',
 'Component/SkeletonPoseComponent','Component/ModelComponent','Component/MotionPlayerComponent',
 'System/TransformSystem','System/CameraSystem','Input/InputTypes','Input/InputSystem',
 'Input/InputBindings','Input/InputConfigLoader','System/MotionSystem','Shader/ModelSkinned'
];
function mask(text) {
 return text.replace(/^\s*#[^\n]*|\/\*[\s\S]*?\*\/|\/\/[^\n]*|R"([^ ()\\\t\r\n]{0,16})\([\s\S]*?\)\1"|"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'/gm,
  s => s.replace(/[^\n]/g, ' '));
}
function pairMap(text) {
 const map = new Map(), stacks = { '{': [], '(': [], '[': [] }, opening = { '}':'{', ')':'(', ']':'[' };
 for (let i=0;i<text.length;i++) {
  if (stacks[text[i]]) stacks[text[i]].push(i);
  else if (opening[text[i]]) { const a=stacks[opening[text[i]]].pop(); if(a!==undefined){map.set(a,i);map.set(i,a);} }
 }
 return map;
}
const ignore = new Set(['if','for','while','switch','catch','sizeof','alignof','decltype','static_assert','return','register']);
function extract(text) {
 const clean=mask(text), pairs=pairMap(clean), found=[];
 let boundary=0;
 for(let i=0;i<clean.length;i++) {
  if(clean[i]===';' || clean[i]==='}') boundary=i+1;
  if(clean[i]!=='{' || !pairs.has(i)) continue;
  const segment=clean.slice(boundary,i);
  // Function bodies can be nested in namespaces/classes, but not in functions.
  const re=/((?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*)\s*\(/g;
  let m, candidate;
  while((m=re.exec(segment))) {
   const open=boundary+m.index+m[0].lastIndexOf('('), close=pairs.get(open);
   if(close===undefined || close>=i || ignore.has(m[1])) continue;
   const tail=clean.slice(close+1,i);
   if(/=>|\[/.test(tail) || /\b(?:class|struct|enum)\b/.test(segment.slice(0,m.index).replace(/template\s*<[^>]*>/g,''))) continue;
   if(!/^\s*(?:const\s*|noexcept(?:\([^)]*\))?\s*|override\s*|final\s*|&\s*|->[^{}]+|:[\s\S]*)*$/.test(tail)) continue;
   candidate={name:m[1],open,close,nameAt:boundary+m.index}; break;
  }
  if(candidate) {
   let start=boundary;
   while(start<i && /\s/.test(clean[start])) start++;
   // Masked comments are spaces; the first non-space starts the actual signature.
   const prefix=clean.slice(start,candidate.nameAt);
   if(/\b(?:public|private|protected)\s*:/.test(prefix)) start+=prefix.lastIndexOf(':')+1;
   while(start<i && /\s/.test(clean[start])) start++;
   const end=pairs.get(i)+1;
   found.push({...candidate,start,end,bodyStart:i,line:text.slice(0,start).split('\n').length,
    signature:text.slice(start,i).trim(),args:text.slice(candidate.open+1,candidate.close).trim(),code:text.slice(start,end)});
   i=end-1; boundary=end;
  } else boundary=i+1;
 }
 return found;
}
const files=[];
for(const stem of stems) for(const ext of ['.h','.cpp','.hlsl']) {
 const rel=stem+ext, full=path.join(src,rel);
 if(!fs.existsSync(full)) continue;
 const text=fs.readFileSync(full,'utf8').replace(/^\uFEFF/,'').replaceAll('\r\n','\n');
 files.push({rel,stem,text,functions:extract(text),slug:rel.replaceAll('/','--')+'.html',hash:crypto.createHash('sha256').update(text).digest('hex')});
}
const docs=new Map();
for(const f of files) {
 const re=/((?:\s*\/\/\/[^\n]*\n)+)\s*([^;{}]+[;{])/g; let m;
 while((m=re.exec(f.text))) {
  const n=m[2].match(/(~?[A-Za-z_]\w*)\s*\(/); if(!n)continue;
  const raw=m[1].replace(/^\s*\/\/\/\s?/gm,'');
  const summary=(raw.match(/<summary>([\s\S]*?)<\/summary>/)||[])[1]?.trim();
  const params=[...raw.matchAll(/<param name="([^"]+)">([\s\S]*?)<\/param>/g)].map(x=>[x[1],x[2].trim()]);
  const returns=(raw.match(/<returns>([\s\S]*?)<\/returns>/)||[])[1]?.trim();
  if(summary) docs.set(f.stem+'::'+n[1],{summary,params,returns});
 }
}
const byName=new Map();
for(const f of files) f.functions.forEach((fn,i)=>{
 fn.id='fn-'+i; fn.file=f;
 const short=fn.name.split('::').at(-1); fn.short=short;
 fn.doc=docs.get(f.stem+'::'+short);
 const list=byName.get(short)||[];list.push(fn);byName.set(short,list);
 fn.calls=[...new Set([...mask(fn.code.slice(fn.code.indexOf('{'))).matchAll(/\b([A-Za-z_]\w*)\s*\(/g)].map(x=>x[1]).filter(x=>!ignore.has(x)))];
});
function linkFn(fn,base='') {return `<a href="${base}files/${fn.file.slug}#${fn.id}">${esc(fn.name)}</a>`;}
function sourceBlock(text,start=1) {
 return `<pre class="source"><code>${text.split('\n').map((line,i)=>`<span class="line"><b>${start+i}</b>${esc(line)}</span>`).join('')}</code></pre>`;
}
function diagram(labels) {
 return `<div class="flow" role="img" aria-label="${esc(labels.join(' → '))}">${labels.map((s,i)=>`${i?'<span class="arrow">→</span>':''}<div>${esc(s).replaceAll('\n','<br>')}</div>`).join('')}</div>`;
}
function ikFigure() {
 return `<figure><svg viewBox="0 0 900 330" role="img" aria-label="肩を固定し、手の直線目標に対して肘の位置と関節回転を解く模式図" style="width:100%;height:auto;background:#f3f7f8;border:1px solid #d5dce1;border-radius:6px"><defs><marker id="arrowhead" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0 L10 5 L0 10Z" fill="#087e80"/></marker></defs><path d="M140 230 L289.968 69.014 L450 220" fill="none" stroke="#a7b3bc" stroke-width="12"/><path d="M140 230 L337.715 133.588 L540 220" fill="none" stroke="#087e80" stroke-width="8"/><path d="M450 220 L540 220" stroke="#d17b25" stroke-width="4" stroke-dasharray="8 6"/><path d="M450 265 L540 265" stroke="#087e80" stroke-width="3" marker-end="url(#arrowhead)"/><g fill="white" stroke="#243640" stroke-width="3"><circle cx="140" cy="230" r="12"/><circle cx="289.968" cy="69.014" r="11"/><circle cx="450" cy="220" r="11"/><circle cx="337.715" cy="133.588" r="11"/><circle cx="540" cy="220" r="12"/></g><g font-family="'Yu Gothic UI',Meiryo,sans-serif" font-size="19" fill="#202831"><text x="48" y="275">肩：固定</text><text x="245" y="35">移動前の肘</text><text x="365" y="100">IKが求めた肘</text><text x="425" y="190">手：開始</text><text x="555" y="218">手：目標</text><text x="430" y="305">手の要求は直線移動</text><text x="630" y="50">灰色：開始姿勢</text><text x="630" y="80">青緑：計算結果</text><text x="630" y="110">※形状は説明用</text></g></svg><figcaption>回転を変えて直線上の目標を実現する。骨の長さと可動域で届かない場合は、直線上の届く位置で止める。</figcaption></figure>`;
}
function renderBlocks(blocks) {return blocks.map(b=>{
 if(b.type==='p')return `<p>${b.html||esc(b.text)}</p>`;
 if(b.type==='h')return `<h2>${esc(b.text)}</h2>`;
 if(b.type==='note')return `<aside><strong>${esc(b.title)}</strong><p>${esc(b.text)}</p></aside>`;
 if(b.type==='flow')return diagram(b.items);
 if(b.type==='ikFigure')return ikFigure();
 if(b.type==='table')return `<div class="tablewrap"><table><thead><tr>${b.head.map(x=>`<th>${esc(x)}</th>`).join('')}</tr></thead><tbody>${b.rows.map(row=>`<tr>${row.map(x=>`<td>${esc(x)}</td>`).join('')}</tr>`).join('')}</tbody></table></div>`;
 if(b.type==='code') {
  const f=files.find(f=>f.rel===b.file); if(!f)throw Error('Missing file '+b.file);
  const fn=f.functions.find(f=>f.name===b.fn || f.short===b.fn);if(!fn)throw Error('Missing function '+b.file+' '+b.fn);
  return `<section class="example"><h3>${esc(b.title||fn.name)}</h3><p class="location">${esc(f.rel)}:${fn.line} · ${linkFn(fn)} の関数別解説へ</p>${sourceBlock(fn.code,fn.line)}${(b.explain||[]).map(x=>`<p>${esc(x)}</p>`).join('')}</section>`;
 }
 return '';
}).join('\n');}
function shell(title,body,base='') {
 return `<!doctype html><html lang="ja"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>${esc(title)} | カスタマイズのコードを読む</title><link rel="stylesheet" href="${base}book.css"></head><body><header><a href="${base}index.html">カスタマイズのコードを読む</a><span>現行実装の教材 / ソース付き</span></header><div class="layout"><nav><a href="${base}index.html">読み方・対象範囲</a>${chapters.map((c,i)=>`<a href="${base}chapter-${i+1}.html">${String(i+1).padStart(2,'0')} ${esc(c.title)}</a>`).join('')}<a href="${base}atlas.html">全ファイル・全関数索引</a><a href="${base}coverage.html">収録範囲・検証記録</a></nav><main><h1>${esc(title)}</h1>${body}<footer>掲載コードは生成時点の実物です。設計の理想像ではなく、現状の責務と副作用を説明しています。</footer></main></div><script src="${base}book.js"></script></body></html>`;
}
function write(name,text){fs.writeFileSync(path.join(out,name),text,'utf8');}
const roleFor=f=>roles[f.stem]||roles[f.stem.split('/').at(-1)]||'カスタマイズから利用する共通基盤。下記の宣言と実装を境界として読みます。';
let detailed=0,commented=0;
for(const f of files) {
 const dependency=[...f.text.matchAll(/^#include "([^"]+)"/gm)].map(m=>m[1]);
 const depHtml=dependency.map(d=>{const target=files.find(x=>x.rel===d);return target?`<a href="${target.slug}">${esc(d)}</a>`:esc(d);}).join(' / ');
 let html=`<p class="lead">${esc(roleFor(f))}</p><p>${f.text.split('\n').length} 行 / 本ファイルの関数定義 ${f.functions.length} 件</p><h2>依存先</h2><p>${depHtml||'標準ヘッダーのみ、または依存宣言なし。'}</p>`;
 const partner=files.find(x=>x.stem===f.stem&&x!==f);if(partner)html+=`<p><a href="${partner.slug}">${esc(partner.rel)} と対で読む</a>。ヘッダーのメンバー変数は、どの状態をこのオブジェクトが保持するかを示します。</p>`;
 html+=`<h2>このファイルの関数</h2><ol>${f.functions.map(fn=>`<li><a href="#${fn.id}">${esc(fn.name)}</a> <small>${fn.line}行</small></li>`).join('')}</ol>`;
 for(const fn of f.functions) {
  const manual=notes[fn.name]||notes[f.stem+'::'+fn.short];
  if(manual)detailed++;if(fn.doc)commented++;
  const d=fn.doc;
  const calls=fn.calls.filter(n=>byName.has(n));
  const refs=calls.map(n=>{
   const candidates=byName.get(n);return candidates.slice(0,6).map(x=>`<a href="${x.file.slug}#${x.id}">${esc(x.name)} (${esc(x.file.rel)})</a>`).join('<br>');
  });
  const returns=[...fn.code.matchAll(/\breturn\s+([^;]+);/g)].map(m=>m[1].trim());
  const guards=[...mask(fn.code).matchAll(/\bif\s*\(/g)].slice(0,12).map(m=>{
   const c=mask(fn.code),p=m.index+m[0].lastIndexOf('('),end=pairMap(c).get(p);return end?fn.code.slice(p+1,end).trim():'';
  }).filter(Boolean);
  html+=`<article id="${fn.id}" class="function"><h2>${esc(fn.name)}</h2><p class="location">${esc(f.rel)}:${fn.line}</p><p>${esc(d?.summary||manual?.[0]||'下の実装を、入力・条件分岐・呼び出し先・出力の順で読みます。個別の意図はこのファイルの役割と本文の処理経路を併せて確認してください。')}</p>`;
  if(manual)html+=manual.map(x=>`<p>${esc(x)}</p>`).join('');
  html+=`<h3>入口と出口</h3><pre class="signature">${esc(fn.signature)}</pre><p><strong>引数の宣言：</strong> ${fn.args?`<code>${esc(fn.args)}</code>`:'なし。メンバー関数なら this が持つ状態を利用できます。'}</p>`;
  if(d?.params.length)html+=`<dl>${d.params.map(([n,t])=>`<dt>${esc(n)}</dt><dd>${esc(t)}</dd>`).join('')}</dl>`;
  html+=`<p><strong>戻り値：</strong> ${esc(d?.returns||(/^\s*(?:inline\s+|static\s+)?void\b/.test(fn.signature)?'値を返しません。参照引数やメンバーの変更、描画・保存などの副作用が結果になります。':'宣言の戻り値型と、下記の return を確認します。コンストラクターは戻り値を宣言しません。'))}</p>`;
  if(returns.length)html+=`<details><summary>実装にある return 式（ラムダ内も含む）</summary><ul>${returns.map(r=>`<li><code>${esc(r)}</code></li>`).join('')}</ul></details>`;
  if(guards.length)html+=`<details><summary>追うべき条件分岐（先頭12件まで）</summary><p>条件が真のときの処理と、早期 return に注目します。文字列は原文のままです。</p><ul>${guards.map(g=>`<li><code>${esc(g)}</code></li>`).join('')}</ul></details>`;
  if(refs.length)html+=`<details><summary>呼び出し先を読む</summary><p>関数名による候補照合です。オーバーロード・別クラスの同名関数は引数と呼び出し元オブジェクトで区別してください。</p><p>${refs.join('<br>')}</p></details>`;
  const callers=files.flatMap(x=>x.functions).filter(x=>x!==fn&&x.calls.includes(fn.short));
  if(callers.length)html+=`<details><summary>この関数を呼ぶ箇所を探す（名前一致の候補 ${callers.length} 件）</summary><p>名前が同じ別関数も含まれます。所有クラスと引数を確認してください。</p><ul>${callers.map(x=>`<li><a href="${x.file.slug}#${x.id}">${esc(x.name)}</a> · ${esc(x.file.rel)}:${x.line}</li>`).join('')}</ul></details>`;
  html+=sourceBlock(fn.code,fn.line)+`</article>`;
 }
 html+=`<h2 id="full-source">宣言・変数・補助定義を含むファイル全文</h2><p>関数の外にある enum、struct、定数、メンバー、プリプロセッサ分岐も省略していません。コメントは原文です。本文の解説と食い違う場合は実装の実行経路を優先してください。</p>${sourceBlock(f.text)}`;
 write('files/'+f.slug,shell(f.rel,html,'../'));
}
chapters.forEach((c,i)=>write(`chapter-${i+1}.html`,shell(c.title,`<p class="lead">${esc(c.goal)}</p>${renderBlocks(c.blocks)}<div class="next">${i+1<chapters.length?`<a href="chapter-${i+2}.html">次へ：${esc(chapters[i+1].title)}</a>`:'<a href="atlas.html">関数索引から実装を読み進める</a>'}</div>`)));
const total=files.reduce((n,f)=>n+f.functions.length,0);
write('atlas.html',shell('全ファイル・全関数索引',`<p>対象 ${files.length} ファイル、抽出した関数定義 ${total} 件。検索はファイル名・関数名・役割を対象にします。宣言だけの関数はヘッダー全文で確認できます。</p><label>検索 <input id="search" placeholder="例：SaveDraft、IK、UIRoot"></label><div id="search-list">${files.map(f=>`<section data-search><h2><a href="files/${f.slug}">${esc(f.rel)}</a></h2><p>${esc(roleFor(f))}</p><ul>${f.functions.map(fn=>`<li>${linkFn(fn)} <small>${fn.line}行</small> ${esc(fn.doc?.summary||'')}</li>`).join('')}</ul></section>`).join('')}</div>`));
write('index.html',shell('カスタマイズのコードを読む',`<p class="lead">画面を操作したとき、どのオブジェクトが何を受け取り、どのデータを書き換え、どう描画・保存されるのか。実コードを追って理解するための教材です。</p><aside><strong>最初に：この資料の読み方</strong><p>01から順に読み、各章の実コードで処理を確認してください。わからない呼び出しは関数別ページへ移動できます。関数別ページには全文を掲載しています。コードを読まずに概要だけで全体を理解したことにはしません。</p></aside>${diagram(['操作する\nボタン・マウス・キー','UIが要求を受ける\nRoot / View','Controllerが下書きを変更\nAttackData / MotionData','プレビューへ反映\n姿勢計算・描画','明示的に保存\nJSON'])}<h2>読み進める順序</h2><ol class="chapters">${chapters.map((c,i)=>`<li><a href="chapter-${i+1}.html">${esc(c.title)}</a><p>${esc(c.goal)}</p></li>`).join('')}</ol><h2>対象と対象外</h2><p>カスタマイズのScene、全Customize Controller、専用UI、編集データ・保存読込、姿勢・IK・制限と、それを支える主要基盤を収録します。対戦ロジック全体やThirdPartyライブラリの全関数解説は対象外です。共通Rendererなどは全文を収録しますが、教材本文ではカスタマイズで使う経路を中心に説明します。</p><p><a href="atlas.html">ファイル・関数索引</a> / <a href="coverage.html">収録範囲と説明の深さを確認</a></p>`));
write('coverage.html',shell('収録範囲・検証記録',`<p>生成日：${new Date().toISOString().slice(0,10)}。対象 ${files.length} ファイル / ${total} 関数定義。個別の補足解説 ${detailed} 件、宣言コメントによる契約説明 ${commented} 件（重複あり）。</p><aside><strong>説明の深さを区別する</strong><p>本文は実行経路と難所を手作業で説明しています。関数別資料は、個別補足・宣言の説明・実コード・条件・返却式・呼び出し候補を掲載しています。すべての関数に同じ深さの手作業解説があるわけではありません。自動抽出された条件・返却式を、人による解説済み件数として数えていません。</p></aside><p>索引はC++の字句と括弧の対応から作成しています。コンパイラーによる意味解析ではありません。全文は全対象ファイルを省略せず掲載し、抽出に含まれない宣言・初期化・ラムダも確認できます。ゲームの動作を今回実行検証したという意味ではありません。</p><table><thead><tr><th>ファイル</th><th>関数定義</th><th>SHA-256（改行正規化後）</th></tr></thead><tbody>${files.map(f=>`<tr><td><a href="files/${f.slug}">${esc(f.rel)}</a></td><td>${f.functions.length}</td><td class="hash">${f.hash}</td></tr>`).join('')}</tbody></table>`));
write('inventory.json',JSON.stringify({files:files.map(f=>({
 path:f.rel,sha256:f.hash,functions:f.functions.map(fn=>({
  name:fn.name,line:fn.line,documented:!!fn.doc,
  individualExplanation:!!(notes[fn.name]||notes[f.stem+'::'+fn.short])
 }))
}))},null,2));
write('book.js',`document.getElementById('search')?.addEventListener('input',e=>{const q=e.target.value.toLowerCase();document.querySelectorAll('[data-search]').forEach(n=>n.hidden=!n.textContent.toLowerCase().includes(q));});`);
write('book.css',`
:root{color-scheme:light;--ink:#202831;--accent:#087e80;--line:#d5dce1}*{box-sizing:border-box}body{margin:0;background:#f7f9fa;color:var(--ink);font-family:'Yu Gothic UI','Meiryo',sans-serif;line-height:1.9;font-size:16px}header{height:64px;background:#fff;border-bottom:1px solid var(--line);display:flex;align-items:center;gap:24px;padding:0 28px}header a{font-weight:700;color:var(--ink);text-decoration:none}header span{font-size:13px;color:#67737b}.layout{display:grid;grid-template-columns:260px minmax(0,1fr);max-width:1700px;margin:auto}nav{padding:26px 18px;position:sticky;top:0;align-self:start;max-height:100vh;overflow:auto}nav a{display:block;padding:8px 10px;font-size:14px;text-decoration:none;border-bottom:1px solid #e2e8eb}a{color:var(--accent);overflow-wrap:anywhere}a:hover{text-decoration:underline}main{background:white;padding:42px 50px;min-width:0;border-left:1px solid var(--line)}h1{font-size:32px;line-height:1.5;margin:0 0 24px}h2{font-size:23px;margin:42px 0 16px;border-bottom:2px solid var(--line);padding-bottom:8px}h3{font-size:18px;margin-top:26px}.lead{font-size:19px;color:#44545c;line-height:2}p{margin:14px 0}aside{background:#ecf6f4;border-left:5px solid var(--accent);padding:18px 22px;margin:26px 0}aside p{margin:8px 0 0}.flow{display:flex;align-items:stretch;flex-wrap:wrap;gap:8px;margin:30px 0;padding:24px 0;border-top:1px solid var(--line);border-bottom:1px solid var(--line)}.flow>div{background:#e8f2f2;border:1px solid #a4c7c8;border-radius:6px;padding:16px;flex:1;min-width:130px;font-size:14px;text-align:center}.arrow{align-self:center;color:var(--accent);font-size:24px}table{border-collapse:collapse;width:100%;font-size:14px}td,th{text-align:left;vertical-align:top;border:1px solid var(--line);padding:10px 14px}th{background:#edf2f4}.tablewrap{overflow:auto}.source{background:#172127;color:#e2ebed;padding:18px 12px;overflow:auto;font:13px/1.85 Consolas,'Yu Gothic UI',monospace;tab-size:4;max-height:680px;border-radius:4px}.source .line{display:block;min-height:1.85em;white-space:pre}.source .line b{display:inline-block;width:55px;text-align:right;padding-right:18px;color:#8fabb7;user-select:none;font-weight:400}.source code{font:inherit}.signature{white-space:pre-wrap;overflow-wrap:anywhere;background:#eef2f5;padding:14px;font-size:13px}.location,small{color:#687c85;font-size:13px}code{overflow-wrap:anywhere;font-family:Consolas,'Yu Gothic UI',monospace}details{border:1px solid var(--line);padding:12px 16px;margin:10px 0;border-radius:4px}summary{cursor:pointer;color:var(--accent);font-weight:600}.function{padding-bottom:24px;border-bottom:4px solid #e7edef;scroll-margin-top:20px}dl{display:grid;grid-template-columns:minmax(90px,180px) 1fr;gap:8px}dt{font-family:Consolas,monospace;color:var(--accent)}dd{margin:0}input{padding:10px 12px;border:1px solid #9caeb7;border-radius:4px;font:inherit;max-width:100%}.hash{font:11px Consolas,monospace;overflow-wrap:anywhere}.next{padding:24px;background:#ecf6f4;margin-top:40px}footer{font-size:12px;color:#6b7a81;border-top:1px solid var(--line);margin-top:50px;padding-top:20px}.chapters li{padding:8px}.chapters p{font-size:14px;margin:4px 0}.example{margin:24px 0}[hidden]{display:none!important}@media(max-width:900px){.layout{grid-template-columns:1fr}nav{position:static;max-height:250px;border-bottom:1px solid var(--line)}main{padding:25px 20px;border:0}header span{display:none}h1{font-size:26px}.flow>div{min-width:170px}}@media print{nav,header{display:none}.layout{display:block}main{padding:0}.source{max-height:none;white-space:pre-wrap;background:#f2f4f5;color:#111}.source .line{white-space:pre-wrap}details{display:block}h2{break-after:avoid}a{color:#222}aside{break-inside:avoid}}
`);
console.log(JSON.stringify({files:files.length,functions:total,individualExplanations:detailed,contracts:commented,chapters:chapters.length},null,2));
