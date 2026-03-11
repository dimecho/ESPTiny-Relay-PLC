var refreshTimer;
var refreshSpeed = 10000;
var saveReminder;
var notifyTimer;

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
var DEMO_PASSWORD = 26;
var TIMEZONE_OFFSET = 27;
var DEMOLOCK = false;
//==========
var redirectURL = location.protocol + '//' + location.host + '.nip.io' + location.pathname;

function notify(messageHeader, messageBody, bg, id) {
    if(bg == 'danger') {
        bg = 'bg-red-500';
    }else if(bg == 'warning') {
        bg = 'bg-yellow-500';
    }else{
        bg = 'bg-green-500';
    }
    var toast = document.createElement('div');
    toast.className = 'px-4 py-1 rounded text-white ' + bg;

    if (messageHeader != '') {
        var toastHeader = document.createElement('div');
        toastHeader.className = 'flex border-b ' + bg;
        toastHeader.textContent = messageHeader;
        
        var btnClose = document.createElement('button');
        btnClose.className = 'flex ml-auto';
        btnClose.textContent = 'X';
        toastHeader.appendChild(btnClose);
        toast.appendChild(toastHeader);
    }

    if (messageBody != '') {
        var toastBody = document.createElement('div');
        toastBody.className = 'toast-body';
        toastBody.textContent = messageBody;
        toast.appendChild(toastBody);
    }
    document.getElementById('notify').appendChild(toast);

    setTimeout(function(toast) {
        document.getElementById('notify').removeChild(toast);
    }, 3600, toast);
}

function saveSetting(offset, value, callback) {

	if(DEMOLOCK) {
		RelayLogin();
	}else{
	    var xhr = new XMLHttpRequest();
	    xhr.onload = function() {
	    	if (xhr.responseText == 'Locked') {
				DEMOLOCK = true;
				RelayLogin();
	    	}else{
	    		DEMOLOCK = false;
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