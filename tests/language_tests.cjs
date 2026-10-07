const fs=require('fs'),vm=require('vm'),assert=require('assert');
const {translateEai}=require('../ui/language-catalog.js');
assert.equal(translateEai('保存動畫',true),'Save Animation');
assert.equal(translateEai('標記起點：83.5',true),'Mark Start: 83.5');
assert.equal(translateEai('保存動畫',false),'保存動畫');
function parent(skip=false){return {closest(){return skip?{}:null}}}
const nodes=[{nodeValue:'保存動畫',parentElement:parent()},{nodeValue:'自訂保存動畫',parentElement:parent(true)}];
const input={attrs:{placeholder:'搜尋骨架名稱'},closest(){return null},hasAttribute(k){return k in this.attrs},getAttribute(k){return this.attrs[k]},setAttribute(k,v){this.attrs[k]=v}};
let onChange,callback,stored='en';const selector={value:'',addEventListener(event,fn){onChange=fn}};
const document={body:{},documentElement:{},getElementById(){return selector},querySelectorAll(){return [input]},createTreeWalker(){let index=-1;return {nextNode(){this.currentNode=nodes[++index];return index<nodes.length}}}};
class MutationObserver{constructor(fn){callback=fn}observe(){}disconnect(){}}
vm.runInNewContext(fs.readFileSync('ui/language.js','utf8'),{document,NodeFilter:{SHOW_TEXT:4},MutationObserver,translateEai,localStorage:{getItem(){return stored},setItem(k,v){stored=v}}});
assert.equal(nodes[0].nodeValue,'Save Animation');assert.equal(nodes[1].nodeValue,'自訂保存動畫');assert.equal(input.attrs.placeholder,'Search Bone Names');
selector.value='zh-TW';onChange();assert.equal(nodes[0].nodeValue,'保存動畫');assert.equal(input.attrs.placeholder,'搜尋骨架名稱');assert.equal(stored,'zh-TW');
selector.value='en';onChange();nodes[0].nodeValue='標記起點：89.0';callback();assert.equal(nodes[0].nodeValue,'Mark Start: 89.0');
callback();assert.equal(nodes[0].nodeValue,'Mark Start: 89.0');
selector.value='zh-TW';onChange();assert.equal(nodes[0].nodeValue,'標記起點：89.0');
assert.equal(document.documentElement.lang,'zh-TW');
console.log('PASS: English/Chinese, persistence, dynamic updates, round-trip text/attributes, protected names');
