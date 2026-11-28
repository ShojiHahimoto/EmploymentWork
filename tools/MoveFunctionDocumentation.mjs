import fs from 'node:fs';
import path from 'node:path';

const root = path.resolve('DrectX11Sample/Source');
const files = [];
function walk(directory) {
    for (const item of fs.readdirSync(directory, { withFileTypes: true })) {
        if (item.name === 'ThirdParty') continue;
        const file = path.join(directory, item.name);
        if (item.isDirectory()) walk(file);
        else if (/\.(h|hpp|cpp)$/.test(file)) files.push(file);
    }
}
walk(root);
const original = new Map(files.map(file => [file, fs.readFileSync(file, 'utf8').replace(/^\uFEFF/, '').replace(/\r\n/g, '\n')]));
const changes = new Map(files.map(file => [file, []]));
function parameters(text) {
    const result = [];
    let depth = 0;
    let current = '';
    for (const c of text) {
        if ('(<[{'.includes(c)) depth++;
        if (')>]}'.includes(c)) depth--;
        if (c === ',' && depth === 0) { result.push(current); current = ''; }
        else current += c;
    }
    if (current.trim()) result.push(current);
    return result.filter(p => p.trim() !== 'void').map(p => {
        const declaration = p.split('=')[0].trim();
        const arrayReference = /\([&*](\w+)\)/.exec(declaration);
        if (arrayReference) return { name: arrayReference[1], type: declaration.replace(arrayReference[1], '').replace(/\s/g, '') };
        const match = /([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?$/.exec(declaration);
        const name = match?.[1] ?? '';
        const type = declaration.slice(0, match?.index ?? declaration.length)
            .replace(/\b(?:DirectX|SimpleMath|std)::/g, '').replace(/\s/g, '');
        return { name, type };
    });
}
const definitions = [];
for (const file of files.filter(f => f.endsWith('.cpp'))) {
    const text = original.get(file);
    const qualifiedRegex = /^([\t ]*)(?!\/|#)([^\n;{}()]*?\b)((?:\w+::\s*)+)(~?\w+)\s*\(([^;{}]*?)\)\s*(const\s*)?(?:noexcept\s*)?\{/gm;
    const freeRegex = /^([\t ]*)(?!\/|#)(?:[\w:<>,*&]+[\t ]+)+?(\w+)\s*\(([^;{}]*?)\)\s*(const\s*)?\{/gm;
    const matches = [...text.matchAll(qualifiedRegex)].map(m => ({ index: m.index, indent: m[1], fullName: m[3].replace(/\s/g, '') + m[4], args: m[5], constant: !!m[6] }));
    for (const m of text.matchAll(freeRegex)) {
        if (!matches.some(x => x.index === m.index)) matches.push({ index: m.index, indent: m[1], fullName: m[2], args: m[3], constant: !!m[4] });
    }
    for (const match of matches) {
        const before = text.slice(0, match.index);
        const doc = /(?:^|\n)((?:[\t ]*\/\/\/[^\n]*\n)+)\s*$/.exec(before);
        const qualified = match.fullName.split('::');
        const name = qualified.pop();
        const open = text.indexOf('(', match.index);
        let close = open + 1;
        let depth = 1;
        while (depth && close < text.length) {
            if (text[close] === '(') depth++;
            if (text[close] === ')') depth--;
            close++;
        }
        const actualArgs = text.slice(open + 1, close - 1);
        definitions.push({ file, start: match.index, scope: qualified.join('::') + '::', name, params: parameters(actualArgs), constant: match.constant,
            doc: doc?.[1] ?? '', docStart: doc ? doc.index + (doc[0].startsWith('\n') ? 1 : 0) : match.index, indent: match.indent });
    }
}
function tags(doc) {
    const xml = doc.replace(/^[\t ]*\/\/\/[ ]?/gm, '');
    return [...xml.matchAll(/<(summary|param|returns|remarks|typeparam)(\s+[^>]*?)?>([\s\S]*?)<\/\1>/g)]
        .map(m => ({ kind: m[1], attrs: m[2] ?? '', body: m[3].trim() }));
}
const unresolved = [];
const moved = [];
let headerDefinitions = 0;
let nonFunctions = 0;
let merged = 0;
for (const file of files.filter(f => /\.(h|hpp)$/.test(f))) {
    const text = original.get(file);
    for (const doc of text.matchAll(/(?:^[\t ]*\/\/\/[^\n]*(?:\n|$))+/gm)) {
        if (!doc[0].includes('<summary>')) continue;
        const tail = text.slice(doc.index + doc[0].length);
        const declaration = /^\s*([^;{]+)([;{])/.exec(tail);
        const fn = declaration && /(~?\w+)\s*\(([^)]*)\)/s.exec(declaration[1]);
        if (!fn) { nonFunctions++; continue; }
        if (declaration[2] === '{') { headerDefinitions++; continue; }
        const params = parameters(fn[2]);
        const prefix = text.slice(0, doc.index);
        const classes = [...prefix.matchAll(/\b(?:class|struct)\s+(\w+)\s*(?:final\s*)?(?::[^;{]*)?\{/g)];
        const owner = classes.at(-1)?.[1];
        let candidates = definitions.filter(d => d.name === fn[1] && d.params.length === params.length);
        candidates = candidates.filter(d => d.constant === /\)\s*const\b/.test(declaration[1]));
        const owned = candidates.filter(d => d.scope.endsWith(`${owner}::`));
        if (owned.length) candidates = owned;
        else {
            const local = candidates.filter(d => path.basename(d.file, '.cpp') === path.basename(file, path.extname(file)));
            if (local.length) candidates = local;
        }
        const exact = candidates.filter(d => d.params.every((p, i) => p.type === params[i].type));
        if (exact.length) candidates = exact;
        if (candidates.length !== 1) {
            unresolved.push({ file: path.relative(root, file), function: fn[1], parameters: params, candidates: candidates.map(d => `${path.relative(root, d.file)}:${d.scope}${d.name}(${d.params.map(p => p.type)})`) });
            continue;
        }
        const target = candidates[0];
        const existing = tags(target.doc);
        const incoming = tags(doc[0]);
        if (target.doc) merged++;
        for (const tag of incoming) {
            if (tag.kind === 'param') {
                const name = /name="([^"]+)"/.exec(tag.attrs)?.[1];
                const index = params.findIndex(p => p.name === name);
                if (index >= 0) tag.attrs = ` name="${target.params[index].name}"`;
            }
            const same = existing.find(e => e.kind === tag.kind && e.attrs.trim() === tag.attrs.trim());
            if (!same) existing.push(tag);
            else if (same.body.replace(/\s/g, '') !== tag.body.replace(/\s/g, '') && tag.kind === 'summary') {
                if (!existing.some(e => e.body === tag.body)) existing.push({ kind: 'remarks', attrs: '', body: tag.body });
            }
            else if (same.body.replace(/\s/g, '') !== tag.body.replace(/\s/g, '') && !same.body.includes(tag.body)) {
                same.body += '\n' + tag.body;
            }
        }
        const rendered = existing.map(t => `<${t.kind}${t.attrs}>${t.body}</${t.kind}>`)
            .join('\n').split('\n').map(line => target.indent + '/// ' + line).join('\n') + '\n';
        changes.get(file).push({ start: doc.index, end: doc.index + doc[0].length, value: '' });
        changes.get(target.file).push({ start: target.docStart, end: target.start, value: rendered });
        moved.push(`${path.relative(root, file)} -> ${path.relative(root, target.file)}::${target.name}`);
    }
}
console.log(JSON.stringify({ moved: moved.length, merged, headerDefinitions, nonFunctions, unresolved }, null, 2));
if (process.argv.includes('--audit')) {
    const problems = [];
    for (const definition of definitions) {
        if (!definition.doc) continue;
        const entries = tags(definition.doc);
        const documented = entries.filter(t => t.kind === 'param').map(t => /name="([^"]+)"/.exec(t.attrs)?.[1]);
        const names = definition.params.map(p => p.name);
        const unknown = documented.filter(name => !names.includes(name));
        if (unknown.length || entries.filter(t => t.kind === 'summary').length > 1) {
            problems.push({ file: path.relative(root, definition.file), function: definition.scope + definition.name, unknown });
        }
    }
    console.log(JSON.stringify({ documentationProblems: problems }, null, 2));
    if (problems.length) process.exitCode = 1;
}
if (unresolved.length) process.exit(1);
if (process.argv.includes('--write')) {
    for (const [file, edits] of changes) {
        if (!edits.length) continue;
        let text = original.get(file);
        for (const edit of edits.sort((a, b) => b.start - a.start)) text = text.slice(0, edit.start) + edit.value + text.slice(edit.end);
        const code = value => value.replace(/^[\t ]*\/\/\/[^\n]*(?:\n|$)/gm, '').replace(/\s/g, '');
        if (code(text) !== code(original.get(file))) throw new Error(`Code changed: ${file}`);
        fs.writeFileSync(file, '\uFEFF' + text.replace(/\n/g, '\r\n'));
    }
    console.log('Moved documentation; non-documentation code verified unchanged.');
}
