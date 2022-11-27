const fs = require('fs')
//eval(fs.readFileSync('./js/beatdetektor.js', e=>console.error(e)))
let timestamp = parseInt(Date.parse(new Date())/1000).toString()
require('./js/beatdetektor')

let windowChk = (!globalThis.hasOwnProperty('window') ? new Object() : window ?? new Object())


function save_output(data){
  let dataF = data.global
  if (dataF.hasOwnProperty('global')){delete dataF.global}
  fs.writeFileSync(timestamp+'_log.json', JSON.stringify(dataF, null, 4), e=>console.error(e))
}

bd_low = new BeatDetektor(48,95);
bd_med = new BeatDetektor(85,169);
bd_high = new BeatDetektor(150,280);
vu = new BeatDetektor.modules.vis.VU();
kick_det = new BeatDetektor.modules.vis.BassKick();

var dummyDataOne = new Array(1024);
var dummyDataTwo = new Array(1024);


// make two simulated buffers for creating a tick, make some random noise with a higher noise in buffer 2
// actual FFT will work better because of proper rising/falling edges, some simulated BPM values will cause failure near BPM boundaries
for (var i = 0; i < 1024; i++) 
{
	if (i < 512)
	{
		dummyDataOne[i] = Math.floor(Math.random()*2.0);
		dummyDataTwo[i] = Math.floor(Math.random()*10.0);
	}
	else
	{
		dummyDataOne[i] = Math.floor(Math.random()*2.0);
		dummyDataTwo[i] = Math.floor(Math.random()*2.0);
	}
}

// simulate bpm
var bpm_sim = 145.5;

// simulate 60fps input
var bdRate = 16;	
var signal_rate = parseInt(((60.0/(bpm_sim))*1000.0));
var signal_counter = 0;
var total_calls = 0;

if (typeof(windowChk.console)!='undefined') console.log("Starting simulation of "+bpm_sim+" BPM.");

// Simulate 1 minute
for (var i = 0; i < 30000; i+=bdRate)
{
	while (signal_counter>signal_rate) signal_counter-=signal_rate;
	bd_low.process((i / 1000.0),(signal_counter < signal_rate*0.1)?dummyDataTwo:dummyDataOne);
	bd_med.process((i / 1000.0),(signal_counter < signal_rate*0.2)?dummyDataTwo:dummyDataOne);
	bd_high.process((i / 1000.0),(signal_counter < signal_rate*0.5)?dummyDataTwo:dummyDataOne);
	
	// module test
	vu.process(bd_med);
	kick_det.process(bd_med);

	// if (kick_det.isKick()) if (typeof(window.console)!='undefined') console.log("Kick @"+(i/1000.0));
	// if (typeof(window.console)!='undefined') console.log("VU @0"+(i/1000.0)+" = "+vu.getLevel(0));
	
	signal_counter+=bdRate;
	total_calls++;
}

if (typeof(windowChk.console)!='undefined') console.log("Total BeatDetektor.process() calls: "+total_calls);

for (var i in bd_med.bpm_contest)
{
	console.log(i+": "+bd_med.bpm_contest[i]);
	
}

let _logs = []

_logs.push({"name": "bd_med-ma_bpm_range","data": bd_med.ma_bpm_range});

_logs.push({"name": "result_low", "data": ((bd_low.win_bpm_int/10.0)+" BPM / "+(bd_low.win_bpm_int_lo)+" BPM")})
_logs.push({"name": "result_med", "data": ((bd_med.win_bpm_int/10.0)+" BPM / "+(bd_med.win_bpm_int_lo)+" BPM")})
_logs.push({"name": "result_high", "data": ((bd_high.win_bpm_int/10.0)+" BPM / "+(bd_high.win_bpm_int_lo)+" BPM")})

console.log(JSON.stringify(_logs, null, 4))
save_output(globalThis)

console.log(`Logs written to ${timestamp}_log.json`)
