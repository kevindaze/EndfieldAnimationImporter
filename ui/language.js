// Display translation only: animation IDs, settings and editable values are untouched.
(() => {
 const select=document.getElementById('languageSelect');
 let language='zh-TW';
 try{language=localStorage.getItem('eai.language')==='en'?'en':'zh-TW'}catch{}
 select.value=language;
 const originals=new WeakMap(),attributes=new WeakMap();
 const excluded=node=>(node.parentElement?.tagName==='OPTION' && /^(clip:|chr_)/.test(node.parentElement.value)) || node.parentElement?.closest('script,style,textarea,pre,#languageSelect,#animationList,[data-language-skip]');
 function refresh(){
  observer.disconnect();
  document.documentElement.lang=language;
  const walker=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT);
  while(walker.nextNode()){
   const node=walker.currentNode;if(excluded(node))continue;
   const previous=originals.get(node);
   const source=previous&&node.nodeValue===previous.display?previous.source:node.nodeValue;
   const display=translateEai(source,language==='en');
   originals.set(node,{source,display});if(node.nodeValue!==display)node.nodeValue=display;
  }
  for(const element of document.querySelectorAll('[placeholder],[aria-label],[title]')){
   if(element.closest('[data-language-skip]'))continue;
   const stored=attributes.get(element)||{};
   for(const name of ['placeholder','aria-label','title'])if(element.hasAttribute(name)){
    const value=element.getAttribute(name),previous=stored[name];
    const source=previous&&value===previous.display?previous.source:value;
    const display=translateEai(source,language==='en');stored[name]={source,display};
    if(value!==display)element.setAttribute(name,display);
   }
   attributes.set(element,stored);
  }
  observer.observe(document.body,{subtree:true,childList:true,characterData:true,attributes:true,attributeFilter:['placeholder','aria-label','title']});
 }
 const observer=new MutationObserver(refresh);
 select.addEventListener('change',()=>{
  language=select.value==='en'?'en':'zh-TW';
  try{localStorage.setItem('eai.language',language)}catch{}
  refresh();
 });
 refresh();
})();
