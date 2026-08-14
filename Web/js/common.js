var refreshTimer;
var refreshSpeed = 10000;
var saveReminder;
var notifyTimer;

//Theme toggle
(function() {
    function applyTheme(dark) {
        document.documentElement.classList.toggle('dark', dark);
        try { localStorage.setItem('theme', dark ? 'dark' : 'light'); } catch(e) {}
    }
    var saved = null;
    try { saved = localStorage.getItem('theme'); } catch(e) {}
    if(saved == 'dark') {
        document.documentElement.classList.add('dark');
    } else if(saved == 'light') {
        document.documentElement.classList.remove('dark');
    } else if(window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) {
        document.documentElement.classList.add('dark');
    }
    var btn = document.createElement('button');
    btn.id = 'theme-toggle';
    btn.setAttribute('aria-label', 'Toggle theme');
    btn.innerHTML = '<svg class="icon-sun" viewBox="0 0 24 24"><circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="3"/><line x1="12" y1="21" x2="12" y2="23"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="1" y1="12" x2="3" y2="12"/><line x1="21" y1="12" x2="23" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/></svg><svg class="icon-moon" viewBox="0 0 24 24"><path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/></svg>';
    btn.addEventListener('click', function() {
        applyTheme(!document.documentElement.classList.contains('dark'));
    });
    document.body.appendChild(btn);
})();

//EEPROM Variables
var WIFI_MODE = 1;
var WIFI_HIDE = 2;
var WIFI_PHY_MODE = 3;
var WIFI_PHY_POWER = 4;
var WIFI_CHANNEL = 5;
var WIFI_SSID = 6;
var WIFI_USERNAME = 7;
var WIFI_PASSWORD = 8;
var LOG_ENABLE = 9;
//==========
var NETWORK_DHCP = 10
var NETWORK_IP = 11;
var NETWORK_SUBNET = 12;
var NETWORK_GATEWAY = 13;
var NETWORK_DNS = 14;
//==========
//var RESERVED = 15;
//var RESERVED = 16;
//var RESERVED = 17;
var GPIO_ARRAY = 18;
var DEEP_SLEEP = 19;
//==========
var EMAIL_ALERT = 20;
var SMTP_SERVER = 21;
var SMTP_USERNAME = 22;
var SMTP_PASSWORD = 23;
var RELAY_NAME = 24;
var ALERTS = 25;
var PLC_PASSWORD = 26;
var TIMEZONE_OFFSET = 27;
var PLCLOCK = false;
//==========
var redirectURL = location.protocol + '//' + location.host + '.nip.io' + location.pathname;

function notify(messageHeader, messageBody, bg, id) {
    var bgClass;
    if(bg == 'danger') {
        bgClass = 'toast-red';
    }else if(bg == 'warning') {
        bgClass = 'toast-yellow';
    }else{
        bgClass = 'toast-green';
    }
    var toast = document.createElement('div');
    toast.className = 'toast ' + bgClass;

    if (messageHeader != '') {
        var toastHeader = document.createElement('div');
        toastHeader.className = 'toast-header';
        toastHeader.textContent = messageHeader;
        
        var btnClose = document.createElement('button');
        btnClose.textContent = 'X';
        toastHeader.appendChild(btnClose);
        toast.appendChild(toastHeader);
    }

    if (messageBody != '') {
        var toastBody = document.createElement('div');
        toastBody.textContent = messageBody;
        toast.appendChild(toastBody);
    }
    document.getElementById('notify').appendChild(toast);

    setTimeout(function(toast) {
        document.getElementById('notify').removeChild(toast);
    }, 3600, toast);
}

function saveSetting(offset, value, callback) {

	if(PLCLOCK) {
		RelayLogin();
	}else{
	    var xhr = new XMLHttpRequest();
	    xhr.onload = function() {
	    	if (xhr.responseText == 'Locked') {
				PLCLOCK = true;
				RelayLogin();
	    	}else{
	    		PLCLOCK = false;
	    	}
	    	if (callback) callback(xhr.responseText);
	    };
	   	xhr.open('GET', '/nvram.json?offset=' + offset + '&value=' + value, true);
	    xhr.send();
	}
}

function RelayLogin() {
	hideAllModals();
	document.getElementById('demo-lock').classList.remove('hidden');
}

function hideAllModals() {
    const backdrop = document.getElementById('modal-backdrop');
    const modals = document.querySelectorAll('.modal');
    modals.forEach(modal => {
        modal.classList.add('hidden');
    });
    backdrop.classList.add('hidden');
}

function RequireInput(id, value) {
    if(value == true) {
        document.getElementById(id).setAttribute('required', '');
    }else{
        document.getElementById(id).removeAttribute('required');
    }
}

function resetFlash()
{
    window.open('/api?reset=1');
}

function progressTimer(speed, bar, callback)
{
    timerUploadCounter = 0;

    var timer = setInterval(function() {
        timerUploadCounter++;
        if(timerUploadCounter == 100) {
            clearInterval(timer);
            if(callback) callback(timerUploadCounter);
        }
        document.getElementsByClassName('progress-bar')[bar].style.width = timerUploadCounter + '%';
    }, speed);
}