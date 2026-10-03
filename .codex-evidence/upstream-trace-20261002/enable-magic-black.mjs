// Native Steam CDP through an authorized SSH tunnel. No web browsing.
import {writeFile} from 'node:fs/promises';
const targets=await (await fetch('http://127.0.0.1:18080/json/list')).json();
async function connect(target){
 const socket=new WebSocket(target.webSocketDebuggerUrl);
 await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});});
 let sequence=0;const pending=new Map();
 socket.addEventListener('message',event=>{const r=JSON.parse(event.data);const p=pending.get(r.id);if(p){pending.delete(r.id);clearTimeout(p.timer);r.error?p.reject(new Error(r.error.message)):p.resolve(r.result);}});
 return {socket,call(method,params={}){return new Promise((resolve,reject)=>{const id=++sequence;const timer=setTimeout(()=>{pending.delete(id);reject(new Error('CDP timeout: '+method));},10000);pending.set(id,{resolve,reject,timer});socket.send(JSON.stringify({id,method,params}));});}};
}
const shared=targets.find(t=>t.title==='SharedJSContext');
if(!shared)throw new Error('Steam SharedJSContext is not ready');
const js=await connect(shared);
try{
 const r=await js.call('Runtime.evaluate',{expression:`(()=>{const plugin=DeckyPluginLoader.plugins.find(p=>p.name==='MagicBlack');if(!plugin)throw new Error('MagicBlack is not loaded');const p=plugin.content.props;p.overlayOpacityState.SetState(1);p.overlayModeState.SetState(1);return {mode:p.overlayModeState.GetState(),opacity:p.overlayOpacityState.GetState()};})()`,returnByValue:true});
 if(r.exceptionDetails)throw new Error('MagicBlack state update failed');
 console.log(JSON.stringify(r.result.value));
}finally{js.socket.close();}
const main=targets.find(t=>t.title==='Steam Big Picture Mode');
if(!main)throw new Error('Steam Big Picture target is not ready');
const page=await connect(main);
try{
 const r=await page.call('Runtime.evaluate',{expression:`(()=>Array.from(document.querySelectorAll('div')).some(e=>{const s=getComputedStyle(e),r=e.getBoundingClientRect();return s.position==='fixed'&&s.backgroundColor==='rgb(0, 0, 0)'&&s.opacity==='1'&&Number(s.zIndex)>=7000&&r.width>=innerWidth&&r.height>=innerHeight;}))()`,returnByValue:true});
 if(r.result.value!==true)throw new Error('Full viewport MagicBlack overlay is not rendered yet');
 console.log('Full viewport black overlay verified');
 const shot=await page.call('Page.captureScreenshot',{format:'png'});
 await writeFile(process.argv[2],Buffer.from(shot.data,'base64'));
}finally{page.socket.close();}
