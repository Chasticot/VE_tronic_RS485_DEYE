#pragma once
const char PILOTAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="fr"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>VE-Tronic · Pilotage solaire</title>
<style>
:root{font:16px system-ui;color:#e7eef8;background:#0e1726;color-scheme:dark}body{max-width:1100px;margin:28px auto;padding:0 18px}h1{font-size:2rem;margin:0}h2{font-size:1.25rem;margin-top:0}h3{margin-bottom:0}section{background:#182538;padding:22px;border:1px solid #33465f;border-radius:14px;margin:18px 0}a{color:#79d9ca}button,input,select{font:inherit;border:1px solid #546680;border-radius:7px;padding:9px;background:#101c2e;color:inherit}button{cursor:pointer;background:#286554;margin:5px}button.stop{background:#a13743}button:disabled{opacity:.5}label{display:block;margin:12px 0}label input:not([type=checkbox]),label select{margin-left:12px;max-width:90%}.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px}.card{background:#101c2e;padding:14px;border-radius:8px}.card strong{display:block;font-size:1.5rem;margin-top:6px}table{width:100%;border-collapse:collapse}td,th{text-align:left;padding:12px 8px;border-bottom:1px solid #33465f}td small{display:block;color:#acbfd7;margin-top:5px}td input{width:120px}pre{white-space:pre-wrap;overflow-wrap:anywhere}#result{position:sticky;top:0;background:#233950;padding:12px;z-index:2;border-radius:6px}p,small{line-height:1.5}.scroll{overflow:auto}.muted{color:#acbfd7}code{background:#101c2e;padding:2px 5px;border-radius:4px}.topbar{display:flex;justify-content:space-between;gap:18px;align-items:start;margin-bottom:13px}.network-state{white-space:nowrap;text-align:right;background:#101c2e;border:1px solid #33465f;border-radius:10px;padding:9px 12px}.network-state b{display:block}.network-state small{display:block}.page-nav{display:flex;flex-wrap:wrap;gap:9px 14px}.page-nav a{text-decoration:none}.network-grid{display:grid;grid-template-columns:1.1fr .9fr;gap:18px}.network-grid section{margin:0}.network-list{display:grid;gap:9px}.network-row{display:flex;align-items:center;justify-content:space-between;gap:12px;background:#101c2e;border:1px solid #33465f;border-radius:9px;padding:11px 13px}.network-row strong{display:block}.network-row small{color:#acbfd7}.network-row button{margin:0}.network-actions{display:flex;gap:8px;align-items:center}.notice{background:#101c2e;border-left:3px solid #79d9ca;padding:11px 13px;border-radius:5px}@media(max-width:680px){body{margin:18px auto;padding:0 12px}.topbar{align-items:stretch;flex-direction:column}.network-state{text-align:left}.network-grid{grid-template-columns:1fr}.network-row{align-items:start;flex-direction:column}.network-actions{width:100%;justify-content:space-between}}
</style>
<header class="topbar"><h1>VE-Tronic <span class="muted">/ solaire</span></h1><div class="network-state" aria-live="polite"><b id="wifiSsid">Wi-Fi : connexion…</b><small id="wifiSignal">Signal indisponible</small></div></header>
<nav class="page-nav" aria-label="Navigation"><a href="/">Console</a><a href="/pilotage">Pilotage</a><a href="/parametres">Modifier les paramètres 🔒</a><a href="/jeedom">Jeedom 🔒</a><a href="/wifi">Réseau & mise à jour 🔒</a></nav>
<p id="result" role="status">Connexion…</p>
<section data-control><h2>Charge du véhicule</h2><p id="status">Lecture des mesures…</p>
<div class="cards" id="cards"></div>
<details><summary>Diagnostic de la régulation</summary><pre id="diagnostic">En attente…</pre></details>
<p><button data-mode="manual">Lancer immédiatement</button><button class="stop" data-mode="stop">Arrêter la charge</button><button data-mode="solar">Solaire dynamique</button><button data-mode="legacy">Rendre la main à la borne / Jeedom</button></p>

<small>Depuis la page Paramètres, le bouton Préparer le pilotage règle « charge_mode » et « solar_mode » à 0 si nécessaire. Les anciens horaires restent mémorisés ; pour les réactiver, rendre la main puis remettre le mode de charge souhaité. Le verrouillage par clef et les protections de la borne restent actifs.</small>
<p>Seuil de démarrage : <b>1 800 W</b> de surplus. Réglage toutes les <b>5 s</b>. Appoint batterie : <b>5 min maximum</b>, puis attente du retour du soleil.</p>
<p>Perte de connexion Deye : dernière consigne maintenue pendant <b>5 min maximum</b>, puis arrêt. Durant ce délai, la charge peut utiliser la batterie ou le réseau. Une temporisation batterie déjà en cours reste prioritaire.</p>
</section>
<section data-settings hidden><h2>Onduleur Deye · connexion</h2>
<form id="settings">
<label>Liaison avec le Deye <select id="deyeTransport" name="transport"><option value="wifi">Wi-Fi · logger Solarman LSW</option><option value="rs485">RS485 · câble direct</option></select></label>
<p>Le Wi-Fi de l'ESP32 reste disponible pour la page web dans les deux modes.</p>
<fieldset id="deyeWifi"><legend>Logger Wi-Fi · TCP 8899, esclave 1</legend>
<label>Adresse IP du logger <input name="host" required placeholder="192.168.1.100"></label>
<label>Numéro de série du logger LSW <input name="serial" required inputmode="numeric"></label>
</fieldset>
<fieldset id="deyeRs485" hidden disabled><legend>RS485 · Modbus RTU</legend>
<label>Adresse Modbus du Deye <input name="slave" type="number" min="1" max="247" value="1" required></label>
<label>Vitesse (bauds) <select name="baud"><option>1200</option><option>2400</option><option>4800</option><option selected>9600</option><option>19200</option><option>38400</option><option>57600</option><option>115200</option></select></label>
<label>Format série <select name="parity"><option value="8N1">8N1 · sans parité</option><option value="8E1">8E1 · parité paire</option><option value="8O1">8O1 · parité impaire</option><option value="8N2">8N2 · 2 bits de stop</option></select></label>
<small>LILYGO T-CAN485 : bornier RS485 A/B vers le port RS485 de supervision du Deye. Réglages initiaux : 9600 bauds, 8N1, adresse 1 ; ils doivent correspondre à l'onduleur. La WB-01 reste sur son adaptateur RS232 (RX32 / TX33).</small>
</fieldset>
<label>Courant maximal autorisé (A) <input name="limit" type="number" min="6" max="63" required></label>
<label>Facteur consommation (registre 178) <select name="loadScale"><option>10</option><option>1</option></select></label>
<label>Facteur réseau (registre 169) <select name="gridScale"><option>10</option><option>1</option></select></label>
<label><input name="pv3" type="checkbox"> Inclure le troisième MPPT (registre 188)</label>
<label><input name="includes" type="checkbox"> La consommation Deye inclut la borne VE</label>
<label><input name="meter" type="checkbox"> J'ai vérifié les puissances Deye et la mesure réelle du courant VE (tore si nécessaire) ; la charge est monophasée.</label>
<button>Enregistrer la configuration</button></form>
<small>Lecture seule des mêmes registres en Wi-Fi et RS485. La mesure du courant VE doit être fiable si la borne est incluse dans la consommation Deye. Vérifier les facteurs avec l'écran de l'onduleur. Le Deye doit déjà autoriser la décharge batterie : cette page ne modifie pas ses réglages batterie. Réserver l'accès HTTP sans certificat au réseau local de confiance.</small></section>
<section data-settings hidden><h2>Paramètres de la WB-01</h2><p>Valeurs et limites lues sur votre borne. Chaque champ permet de modifier le paramètre de la même ligne. Arrêter la charge avant modification. Le firmware valide également les valeurs et les droits d'accès.</p>
<label>Mot de passe système WB-01 <input id="pin" type="password" inputmode="numeric" maxlength="6" autocomplete="off" placeholder="6 chiffres"></label>
<p><button id="prepare">Préparer le pilotage</button> Configure charge_mode=0 et solar_mode=0 pour permettre les démarrages sans mot de passe depuis Pilotage.</p>
<button id="reload">Lire tous les paramètres</button><div class="scroll"><table><thead><tr><th>Paramètre</th><th>Valeur actuelle</th><th>Nouvelle valeur</th></tr></thead><tbody id="parameters"></tbody></table></div>
<h3>Date et heure de la borne</h3><pre id="dateState">Non lue</pre><form id="dateForm"><input name="value" type="datetime-local" min="2012-01-01T00:00" max="2038-12-31T23:59" required><button>Régler l'heure</button></form>
</section>
<section data-console hidden><h2>Console historique</h2>
<p>Envoyer une commande brute à la borne. Les lectures courantes (<code>list</code>, <code>date</code>, <code>help</code>, <code>version</code>, <code>evse_state</code>, <code>$GG*B2</code>, <code>info</code>, <code>get</code>, <code>$SC</code> valide…) sont accessibles sans mot de passe ; toute autre commande demande l'authentification.</p>
<p>Retour au format XML sur le port TCP : <a id="tcpLink" href="#">chargement…</a></p>
<form id="commandForm"><label>Commande <input name="commande" id="commandeInput" placeholder="help"></label><button>Envoyer</button></form>
<pre id="commandResult"></pre>
</section>
<section data-network hidden><h2>Réseau Wi-Fi</h2><p id="wifiDetail" class="notice">Lecture de l'état réseau…</p>
<div class="network-grid"><section><h3>Se connecter à un réseau</h3><p class="muted">Le réseau est mémorisé puis la borne bascule vers lui. La page peut se fermer pendant le changement de connexion.</p><form id="wifiForm"><label>Nom du réseau (SSID) <input name="ssid" id="wifiName" maxlength="31" required autocomplete="off"></label><label>Mot de passe <input name="password" type="password" maxlength="63" autocomplete="new-password"></label><button>Enregistrer et se connecter</button></form><p><button id="scanWifi" type="button">Actualiser les réseaux détectés</button></p><div id="wifiScan" class="network-list"><small>Recherche des réseaux…</small></div></section><section><h3>Réseaux mémorisés</h3><p class="muted">Les mots de passe ne sont jamais affichés.</p><div id="wifiSaved" class="network-list"><small>Chargement…</small></div></section></div></section>
<section data-network hidden><h2>Mise à jour du firmware</h2><p class="notice">Choisissez le fichier <code>.bin</code> du firmware. L'alimentation ne doit pas être coupée pendant l'envoi ; la borne redémarrera automatiquement après vérification.</p><form id="firmwareForm"><label>Fichier firmware <input name="firmware" type="file" accept=".bin,application/octet-stream" required></label><button>Installer la mise à jour</button></form></section>
<section data-jeedom hidden><h2>Configuration Jeedom</h2>
<p>Envoi périodique du status de la borne vers Jeedom par API HTTP.</p>
<form id="jeedomForm">
<label>Adresse IP de Jeedom <input name="ip_jeedom" pattern="^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"></label>
<label>Clé API <input name="api_key_jeedom"></label>
<label>ID Code Status <input name="id_code_status" type="number"></label>
<label>ID Texte Status <input name="id_txt_status" type="number"></label>
<label>ID Tension <input name="id_tension" type="number"></label>
<label>ID Courant <input name="id_courant" type="number"></label>
<label>ID Courant Max <input name="id_courant_max" type="number"></label>
<label>Fréquence de rafraîchissement (ms, 0 = désactivé, mini 2000) <input name="freq_update_evse" type="number"></label>
<button>Enregistrer</button></form>
</section>
<script>
let token='',initialized=false,busy=false;const $=id=>document.getElementById(id);
const editing=location.pathname==='/parametres', isConsole=location.pathname==='/', isJeedom=location.pathname==='/jeedom', isWifi=location.pathname==='/wifi';
document.querySelectorAll('[data-settings]').forEach(e=>{if(editing)e.hidden=false;else e.remove()});
document.querySelectorAll('[data-console]').forEach(e=>{if(isConsole)e.hidden=false;else e.remove()});
document.querySelectorAll('[data-jeedom]').forEach(e=>{if(isJeedom)e.hidden=false;else e.remove()});
document.querySelectorAll('[data-network]').forEach(e=>{if(isWifi)e.hidden=false;else e.remove()});
if(editing||isConsole||isJeedom||isWifi){const c=document.querySelector('[data-control]');if(c)c.hidden=true;}
if(editing)document.title='VE-Tronic · Paramètres';
else if(isConsole)document.title='VE-Tronic · Console';
else if(isJeedom)document.title='VE-Tronic · Jeedom';
else if(isWifi)document.title='VE-Tronic · Réseau & mise à jour';

function message(s){$('result').textContent=s}
function deyeFields(){const selector=$('deyeTransport');if(!selector)return;const rs=selector.value==='rs485';for(const [id,active]of [['deyeWifi',!rs],['deyeRs485',rs]]){const group=$(id);if(group){group.hidden=!active;group.disabled=!active}}}
if($('deyeTransport'))$('deyeTransport').onchange=deyeFields;
async function api(path,data){const r=await fetch(path,{method:data?'POST':'GET',headers:data?{'X-CSRF-Token':token}: {},body:data?new URLSearchParams(data):undefined,cache:'no-store'});if(!r.ok)throw Error(await r.text());return r.headers.get('content-type')?.includes('application/json')?r.json():r.text()}
async function action(fn){if(busy)return;busy=true;try{await fn()}catch(e){message(e.message)}finally{busy=false}}
async function status(){try{const d=await api('/api/status');token=d.token;if($('wifiSsid'))$('wifiSsid').textContent=d.wifiConnected?'Wi-Fi : '+d.wifiSsid:'Wi-Fi : non connecté';if($('wifiSignal'))$('wifiSignal').textContent=d.wifiConnected?d.wifiPercent+' % · '+d.wifiRssi+' dBm':'Signal indisponible';if($('diagnostic'))$('diagnostic').textContent=['Mode ESP32 : '+d.mode,'Liaison Deye : '+(d.transport==='rs485'?'RS485 · '+d.baud+' bauds · '+d.parity+' · adresse '+d.slave:'Wi-Fi · LSW'),'Lecture Deye : '+(d.deyeError||'OK'),'Surplus calculé : '+d.surplusW+' W','Limite WB-01 : '+d.nativeLimitA+' A','Dernière demande : $SC '+d.requestedA,'Échange $SC : '+(d.currentDelivered?'terminé':'non confirmé'),'Limite appliquée : '+(d.currentConfirmed?'confirmée par relecture':'à vérifier / limitée par la borne'),'Tension prise : '+d.volts+' V','Tension de calcul : '+d.calculationVolts+' V'+(d.voltageReference?' (référence : mesure WB-01 non exploitable)':''),'Relecture après $SC : '+(d.currentReadback||'—'),'Lecture borne : '+(d.wbReadError||'OK'),'Réponse $SC : '+(d.currentReply||'—'),'Réponse evse_state : '+(d.stateReply||'—'),'Réponse $GG*B2 : '+(d.valuesReply||'—')].join('\n');$('status').textContent=d.message+' · '+(d.wbValid?['Véhicule débranché','Véhicule branché','Véhicule en charge'][d.state]:'Borne indisponible')+(d.bridgeSeconds?' · Appoint restant : '+d.bridgeSeconds+' s':'');const metrics=[['Production PV',d.deyeValid?d.pv+' W':'—'],['Consommation Deye',d.deyeValid?d.load+' W':'—'],['Batterie (+ décharge)',d.deyeValid?d.battery+' W / '+d.soc+' %':'—'],['Réseau (+ import)',d.deyeValid?d.grid+' W':'—'],['Courant VE',d.wbValid?d.amps.toFixed(1)+' A':'—'],['Consigne demandée',d.currentDelivered?(d.targetA<0?'Borne':d.targetA+' A'+(d.currentConfirmed?' ✓':' (à vérifier)')):'Non transmise']];if($('cards'))$('cards').replaceChildren(...metrics.map(([label,value])=>{const el=document.createElement('div');el.className='card';el.textContent=label;const v=document.createElement('strong');v.textContent=value;el.append(v);return el}));if(!initialized){for(const [k,v]of Object.entries(d)){const e=$('settings')?.elements.namedItem(k);if(e){if(e.type==='checkbox')e.checked=v;else e.value=v}}deyeFields();initialized=true;message('Connexion établie.');if($('tcpLink')){const url=`http://${location.hostname}:${d.tcpPort}/commande_Vetronic`;$('tcpLink').textContent=url;$('tcpLink').href=url}if($('commandeInput'))$('commandeInput').value=d.lastCommand;if(editing)await parameters();if(isJeedom)await jeedomInit();if(isWifi)await wifiInit()}}catch(e){if($('status'))$('status').textContent='Connexion perdue : mesures non actualisées';message(e.message)}}
async function parameters(){const d=await api('/api/parameters');$('dateState').textContent=d.date;$('parameters').replaceChildren(...d.parameters.map(p=>{const tr=document.createElement('tr'),name=document.createElement('td'),value=document.createElement('td'),edit=document.createElement('td'),desc=document.createElement('small'),input=document.createElement('input'),button=document.createElement('button');name.textContent=p.name;desc.textContent=p.description+' · Limites : '+p.min+' à '+p.max+' '+p.unit;name.append(desc);value.textContent=p.value+' '+p.unit;input.value=p.value;input.setAttribute('aria-label','Nouvelle valeur '+p.name);button.textContent='Appliquer';button.onclick=()=>action(async()=>{const result=await api('/api/parameter',{name:p.name,value:input.value,pin:$('pin').value});message(result);await parameters()});edit.append(input,button);tr.append(name,value,edit);return tr}))}
async function jeedomInit(){const d=await api('/api/jeedom');for(const [k,v]of Object.entries(d)){const e=$('jeedomForm')?.elements.namedItem(k);if(e)e.value=v}}
function networkRow(name,detail,buttonText,click){const row=document.createElement('div'),info=document.createElement('div'),title=document.createElement('strong'),small=document.createElement('small'),actions=document.createElement('div'),button=document.createElement('button');row.className='network-row';actions.className='network-actions';title.textContent=name;small.textContent=detail;button.textContent=buttonText;button.onclick=click;info.append(title,small);actions.append(button);row.append(info,actions);return row}
async function wifiInit(){const d=await api('/api/wifi');$('wifiDetail').textContent=d.connected?'Connecté à '+d.ssid+' · '+d.percent+' % ('+d.rssi+' dBm) · '+d.ip:'Aucun réseau Wi-Fi connecté.';const saved=$('wifiSaved');const savedRows=d.saved.length?d.saved.map(n=>networkRow(n,n===d.ssid?'Réseau actif — mémorisé':'Réseau mémorisé','Supprimer',()=>action(async()=>{message(await api('/api/wifi',{action:'delete',ssid:n}));await wifiInit()}))):[Object.assign(document.createElement('small'),{textContent:'Aucun réseau mémorisé.'})];saved.replaceChildren(...savedRows);const scan=$('wifiScan');const scanRows=d.networks.length?d.networks.map(n=>networkRow(n.ssid,n.percent+' % · '+n.rssi+' dBm'+(n.secured?' · sécurisé':' · ouvert'),'Choisir',()=>{$('wifiName').value=n.ssid;$('wifiForm').elements.password.focus()})):[Object.assign(document.createElement('small'),{textContent:'Aucun réseau détecté.'})];scan.replaceChildren(...scanRows);}
document.querySelectorAll('[data-mode]').forEach(b=>b.onclick=()=>action(async()=>{message(await api('/api/mode',{mode:b.dataset.mode}));await status()}));
if($('settings'))$('settings').onsubmit=e=>{e.preventDefault();action(async()=>{const data=Object.fromEntries(new FormData(e.target));for(const k of ['pv3','includes','meter'])data[k]=e.target.elements[k].checked?'1':'0';message(await api('/api/config',data))})};
if($('dateForm'))$('dateForm').onsubmit=e=>{e.preventDefault();action(async()=>{message(await api('/api/date',Object.fromEntries(new FormData(e.target))));await parameters()})};
if($('reload'))$('reload').onclick=()=>action(parameters);
if($('prepare'))$('prepare').onclick=()=>action(async()=>{message(await api('/api/prepare',{pin:$('pin').value}));await parameters()});
if($('commandForm'))$('commandForm').onsubmit=e=>{e.preventDefault();action(async()=>{const commande=e.target.elements.commande.value;const r=await api('/api/command',{commande});$('commandResult').textContent=r;message('Commande envoyée.')})};
if($('jeedomForm'))$('jeedomForm').onsubmit=e=>{e.preventDefault();action(async()=>{message(await api('/api/jeedom',Object.fromEntries(new FormData(e.target))))})};
if($('wifiForm'))$('wifiForm').onsubmit=e=>{e.preventDefault();action(async()=>{message(await api('/api/wifi',Object.assign({action:'connect'},Object.fromEntries(new FormData(e.target)))));$('wifiDetail').textContent='Basculement vers le nouveau réseau en cours…'})};
if($('scanWifi'))$('scanWifi').onclick=()=>action(wifiInit);
if($('firmwareForm'))$('firmwareForm').onsubmit=e=>{e.preventDefault();action(async()=>{const file=e.target.elements.firmware.files[0];if(!file)throw Error('Sélectionnez un fichier .bin.');message('Envoi de '+file.name+'… ne fermez pas cette page.');const form=new FormData();form.append('firmware',file);const r=await fetch('/api/update',{method:'POST',headers:{'X-CSRF-Token':token},body:form});const text=await r.text();if(!r.ok)throw Error(text);message(text)})};
async function poll(){if(!busy)await status();setTimeout(poll,5000)}poll();
</script></html>)HTML";
