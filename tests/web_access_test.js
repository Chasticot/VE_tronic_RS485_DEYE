// Regression: the public dashboard must not request protected settings endpoints.
const fs = require('fs');
const vm = require('vm');
const assert = require('assert/strict');
const source = fs.readFileSync(require('path').join(__dirname, '../VETRONIC_RS485/pilotage_page.h'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];
assert(source.includes('name="socGuard"'));
assert(source.includes('name="socStop"'));
assert(source.includes('name="socResume"'));
assert(source.includes('id="ledSettings"'));
assert(source.includes('id="manualAmps" type="number" min="6" max="63"'));
assert(source.includes('id="deyeState"'));
assert(script.includes("wifiInit(scan=false)"));
assert(script.includes("action:'scan'"));
assert(script.includes("wifiInit(true)"));
assert(script.includes("Array.isArray(d.saved)"));
assert(script.includes("Array.isArray(d.networks)"));
assert(script.includes("api('/api/led',data)"));
assert(script.includes("['settings','ledSettings']"));
async function exercise(editing,transport='wifi') {
  const ids = new Map();
  const node = () => ({textContent:'', hidden:false, append(){}, replaceChildren(){}, setAttribute(){}, elements:{namedItem(){return null;}}});
  for(const id of ['result','status','deyeState','cards','settings','dateState','parameters','pin','dateForm','reload','prepare','manualAmps']) ids.set(id,node());
  ids.get('manualAmps').value='16';
  if(editing){
    for(const id of ['deyeTransport','deyeWifi','deyeRs485']) ids.set(id,node());
    ids.get('settings').elements.namedItem=k=>k==='transport'?ids.get('deyeTransport'):null;
  }
  ids.get('pin').value='123456';
  const settings = [{remove(){for(const k of ['settings','dateState','parameters','pin','dateForm','reload','prepare']) ids.delete(k);}}];
  const mode = {...node(),dataset:{mode:'manual'}};
  const calls=[];
  const sandbox={location:{pathname:editing?'/parametres':'/pilotage'},URLSearchParams,setTimeout(){},
    document:{getElementById:id=>ids.get(id)||null, createElement:node, querySelector:node,
      querySelectorAll:selector=>selector==='[data-settings]'?settings:selector==='[data-mode]'?[mode]:[]},
    fetch:async(path,options)=>{
      calls.push({path,options});
      const data=path==='/api/status'?{token:'test-token',message:'OK',wbValid:true,state:1,amps:0,targetA:0,deyeValid:true,deyeUsingLastGood:true,deyeAgeSeconds:6,deyeError:'Connexion Deye impossible',transport}: {parameters:[],date:'2026'};
      return {ok:true,headers:{get:()=>options.method==='GET'?'application/json':'text/plain'},json:async()=>data,text:async()=>'OK'};
    }
  };
  vm.runInNewContext(script,sandbox);
  await new Promise(setImmediate);
  if(!editing) {
    assert.deepEqual(calls.map(c=>c.path),['/api/status']);
    assert.equal(ids.has('pin'),false);
    await mode.onclick();
    const request=calls.find(c=>c.path==='/api/mode');
    assert.equal(request.options.body.get('mode'),'manual');
    assert.equal(request.options.body.get('amps'),'16');
    assert.equal(request.options.body.has('pin'),false);
    assert.equal(request.options.headers['X-CSRF-Token'],'test-token');
    assert(!calls.some(c=>['/api/parameters','/api/config','/api/prepare'].includes(c.path)));
  } else {
    assert.deepEqual(calls.map(c=>c.path),['/api/status','/api/parameters']);
    assert.equal(ids.get('deyeTransport').value,transport);
    const checkFields=rs=>{
      assert.equal(ids.get('deyeWifi').hidden,rs);
      assert.equal(ids.get('deyeWifi').disabled,rs);
      assert.equal(ids.get('deyeRs485').hidden,!rs);
      assert.equal(ids.get('deyeRs485').disabled,!rs);
    };
    checkFields(transport==='rs485');
    for(const value of ['rs485','wifi','rs485']){
      ids.get('deyeTransport').value=value;
      ids.get('deyeTransport').onchange();
      checkFields(value==='rs485');
    }
    await ids.get('prepare').onclick();
    assert.equal(calls.find(c=>c.path==='/api/prepare').options.body.get('pin'),'123456');
  }
}
(async()=>{await exercise(false);await exercise(true);await exercise(true,'rs485');console.log('OK : pilotage public, paramètres protégés, jeton POST, restauration et sélection Wi-Fi/RS485.');})().catch(e=>{console.error(e);process.exitCode=1;});
