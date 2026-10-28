const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync('ui/index.html','utf8');
const nodes=new Map();
function node(id){if(!nodes.has(id))nodes.set(id,{value:'',options:[],textContent:'',hidden:false,attrs:{},setAttribute(k,v){this.attrs[k]=v},replaceChildren(){this.options=[]},append(option){this.options.push(option)}});return nodes.get(id)}
const calls=[];
const context={document:{getElementById:node,createElement(){return {value:'',textContent:''}}},send(command,fields){calls.push({command,fields});return Promise.resolve({spawn_character:{character:fields?.character||'chr_0036_jsspsi',characters:[{id:'chr_0036_jsspsi',name:'祀'},{id:'chr_0016_laevat',name:'萊萬汀'}],status:'ok'}})}};
vm.createContext(context);
vm.runInContext(html.match(/function switchPage\(animation\)\{[^\n]+/)[0],context);
context.switchPage('Spawn');assert.equal(node('pageSpawn').hidden,false);assert.equal(node('pageBreast').hidden,true);
vm.runInContext(html.match(/function renderSpawn\(data\)\{[^\n]+/)[0],context);
context.renderSpawn({character:'chr_0016_laevat',characters:[{id:'chr_0036_jsspsi',name:'祀'},{id:'chr_0016_laevat',name:'萊萬汀'}],status:'ready'});
assert.equal(node('spawnCharacterSelect').options.length,2);assert.equal(node('spawnCharacterSelect').value,'chr_0016_laevat');
node('spawnCharacter').onclick();assert.equal(calls.at(-1).command,'spawn_character_create');assert.equal(calls.at(-1).fields.character,'chr_0016_laevat');
node('removeSpawnedCharacter').onclick();assert.equal(calls.at(-1).command,'spawn_character_remove');
assert(html.indexOf('id="tabSpawn"')>html.indexOf('id="tabBreast"'));
console.log('PASS: rightmost spawn tab, operator selection, spawn payload and remove control');
