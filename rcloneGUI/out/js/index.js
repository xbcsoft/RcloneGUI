/**
 * rcloneGUI UI Logic for Sciter
 * Implements Multi-language (ZH/EN, default ZH), Slide-in Drawer Animations,
 * NAS Tab Config (WebDAV / SFTP / FTP) & Real-time Subprocess Native Bridging.
 */

var I18N = {};
var currentLanguage = "zh";
var currentProtocol = "WebDAV";
var selectedDriveLetter = "Auto";
var mountedDrives = [];

// ==========================================================================
// 1. Dynamic Multi-language Discovery & Loading
// ==========================================================================
async function loadAllLanguages(targetLang) {
    var langFiles = ["zh.js", "en.js"];
    if (typeof Native_GetLanguageFiles === "function") {
        try {
            var files = Native_GetLanguageFiles();
            if (Array.isArray(files) && files.length > 0) {
                langFiles = files;
            }
        } catch (e) {
            console.error("Native_GetLanguageFiles error:", e);
        }
    }

    var newI18N = {};
    for (var i = 0; i < langFiles.length; i++) {
        var fName = langFiles[i];
        var code = fName.replace(/\.js$/i, "");
        try {
            var mod = await import("../lang/" + fName);
            newI18N[code] = mod.default || mod;
        } catch (err) {
            console.error("Failed to load language module:", fName, err);
        }
    }

    if (Object.keys(newI18N).length > 0) {
        I18N = newI18N;
    }

    // 动态同步填充设置界面的 <select id="selLanguage">
    var sel = document.getElementById("selLanguage");
    if (sel) {
        var curVal = sel.value || targetLang || currentLanguage;
        sel.innerHTML = "";
        for (var langCode in I18N) {
            var langObj = I18N[langCode];
            var displayName = langObj.lang_display_name || langObj.lang_name || (langCode === "zh" ? "简体中文 (Chinese)" : (langCode === "en" ? "English" : langCode));
            var opt = document.createElement("option");
            opt.value = langCode;
            opt.innerText = displayName;
            sel.appendChild(opt);
        }
        sel.value = I18N[curVal] ? curVal : (I18N["zh"] ? "zh" : Object.keys(I18N)[0]);
    }

    // 动态同步填充 <popup id="popupLanguage">
    var popup = document.getElementById("popupLanguage");
    if (popup) {
        popup.innerHTML = "";
        for (var codeKey in I18N) {
            var itemObj = I18N[codeKey];
            var nameStr = itemObj.lang_display_name || itemObj.lang_name || codeKey;
            var div = document.createElement("div");
            div.className = "gui-popup-item";
            div.setAttribute("onclick", "chooseLanguage('" + codeKey + "')");
            div.innerText = nameStr;
            popup.appendChild(div);
        }
    }

    setLanguage(targetLang || currentLanguage || "zh");
}

async function refreshLanguages() {
    await loadAllLanguages(currentLanguage);
}

// ==========================================================================
// 2. Language Switcher Engine
// ==========================================================================
function setLanguage(lang) {
    if (!I18N[lang]) {
        if (I18N["zh"]) lang = "zh";
        else if (Object.keys(I18N).length > 0) lang = Object.keys(I18N)[0];
    }
    currentLanguage = lang;
    var dict = I18N[lang] || {};

    var transElements = document.querySelectorAll("[data-i18n]");
    for (var i = 0; i < transElements.length; i++) {
        var el = transElements[i];
        var key = el.getAttribute("data-i18n");
        if (dict[key]) {
            el.innerText = dict[key];
        }
    }

    var phElements = document.querySelectorAll("[data-i18n-ph]");
    for (var j = 0; j < phElements.length; j++) {
        var phel = phElements[j];
        var phKey = phel.getAttribute("data-i18n-ph");
        if (dict[phKey]) {
            phel.setAttribute("placeholder", dict[phKey]);
        }
    }

    var langTriggerVal = document.getElementById("langDropdownVal");
    if (langTriggerVal) {
        langTriggerVal.innerText = dict.lang_display_name || dict.lang_zh || dict.lang_en || lang;
    }
    var selLang = document.getElementById("selLanguage");
    if (selLang) {
        selLang.value = lang;
    }

    var titleEls = document.querySelectorAll("[data-i18n-title]");
    for (var k = 0; k < titleEls.length; k++) {
        var tKey = titleEls[k].getAttribute("data-i18n-title");
        if (dict[tKey]) {
            titleEls[k].setAttribute("title", dict[tKey]);
        }
    }

    // 标题栏刷新按钮单独兜底，确保语言切换后文本与提示同步更新
    var refreshBtn = document.getElementById("btnRefresh");
    if (refreshBtn) {
        var refreshText = refreshBtn.querySelector("[data-i18n=\"btn_refresh\"]");
        if (refreshText) refreshText.innerText = dict.btn_refresh || (lang === "zh" ? "刷新" : "Refresh");
        refreshBtn.setAttribute("title", dict.btn_refresh || (lang === "zh" ? "刷新" : "Refresh"));
    }

    refreshDriveListView();
    updateServiceStatus();
}

function updateServiceStatus(forcedState) {
    var lbl = document.getElementById("lblServiceStatus");
    if (!lbl) return;
    var statusCode = 0; // 0: 未安装, 1: 已启动, 2: 未启动
    if (typeof forcedState === "number") {
        statusCode = forcedState;
    } else if (typeof forcedState === "object" && forcedState !== null) {
        if (typeof forcedState.statusCode === "number") {
            statusCode = forcedState.statusCode;
        } else if (forcedState.isInstalled) {
            statusCode = forcedState.isRunning ? 1 : 2;
        }
    } else if (typeof Native_GetServiceStatus === "function") {
        try {
            var res = Native_GetServiceStatus();
            if (res && typeof res.statusCode === "number") {
                statusCode = res.statusCode;
            } else if (res && res.isInstalled) {
                statusCode = res.isRunning ? 1 : 2;
            }
        } catch(e) {}
    }
    
    var dict = I18N[currentLanguage] || I18N["zh"] || I18N["en"] || {};
    if (statusCode === 1) {
        lbl.innerText = dict.service_status_running || "服务已安装 (已启动)";
        lbl.style.color = "#0082a6";
    } else if (statusCode === 2) {
        lbl.innerText = dict.service_status_stopped || "服务已安装 (未启动)";
        lbl.style.color = "#666666";
    } else {
        lbl.innerText = dict.service_status_not_installed || "未安装系统服务";
        lbl.style.color = "#666666";
    }
}

// ==========================================================================
// 3. Sliding Drawer / Flyout Switcher Engine
// ==========================================================================
function slideIn(panelId) {
    var flyouts = document.querySelectorAll(".metro-flyout-drawer");
    for (var i = 0; i < flyouts.length; i++) {
        if (flyouts[i].id !== panelId) {
            flyouts[i].classList.remove("active");
        }
    }

    var target = document.getElementById(panelId);
    if (target) {
        target.classList.add("active");
        if (panelId === "viewSettings") {
            loadSettingsFromNative();
            updateServiceStatus();
        }
    }
}

function slideOut(panelId) {
    var target = document.getElementById(panelId);
    if (target) {
        target.classList.remove("active");
    }
}

// ==========================================================================
// 4. Real-time Required Field Border Validation Engine
// ==========================================================================
function checkInputValidation() {
    var hostInput = document.getElementById("inputHost");
    var userInput = document.getElementById("inputUsername");
    var passInput = document.getElementById("inputPassword");
    var anonChk = document.getElementById("chkAnonymous");

    if (hostInput) {
        if (hostInput.value && hostInput.value.trim().length > 0) {
            hostInput.classList.remove("has-red-border");
        } else {
            hostInput.classList.add("has-red-border");
        }
    }

    var isAnon = anonChk && anonChk.checked;

    if (userInput) {
        if (isAnon || (userInput.value && userInput.value.trim().length > 0)) {
            userInput.classList.remove("has-red-border");
        } else {
            userInput.classList.add("has-red-border");
        }
    }

    if (passInput) {
        if (isAnon || (passInput.value && passInput.value.trim().length > 0)) {
            passInput.classList.remove("has-red-border");
        } else {
            passInput.classList.add("has-red-border");
        }
    }
}

function setInputValue(inputEl, val) {
    if (!inputEl) return;
    inputEl.value = val;
    try {
        var editor = inputEl.edit || inputEl.password || inputEl.textarea;
        var len = val ? val.length : 0;
        if (editor && typeof editor.selectRange === "function") {
            editor.selectRange(len, len);
        } else if (typeof inputEl.setSelectionRange === "function") {
            inputEl.setSelectionRange(len, len);
        } else if ("selectionStart" in inputEl) {
            inputEl.selectionStart = len;
            inputEl.selectionEnd = len;
        }
    } catch (e) {}
}

// ==========================================================================
// 6. SSL & Anonymous Checkbox Handlers
// ==========================================================================
function onSSLChange(chk) {
    var sslChk = document.getElementById("chkSSL");
    var isChecked = sslChk ? sslChk.checked : (chk && chk.checked);
    var protoLabel = document.getElementById("lblProtocol");
    
    if (currentProtocol === "FTP") {
        if (protoLabel) protoLabel.innerText = isChecked ? "ftps://" : "ftp://";
    } else if (currentProtocol === "WebDAV") {
        if (protoLabel) protoLabel.innerText = isChecked ? "https://" : "http://";
    } else if (currentProtocol === "SFTP") {
        if (protoLabel) protoLabel.innerText = "sftp://";
    }
}

function onAnonymousChange(chk) {
    var userIn = document.getElementById("inputUsername");
    var passIn = document.getElementById("inputPassword");
    if (chk.checked) {
        if (userIn) { userIn.setAttribute("disabled", ""); userIn.style.opacity = "0.4"; }
        if (passIn) { passIn.setAttribute("disabled", ""); passIn.style.opacity = "0.4"; }
    } else {
        if (userIn) { userIn.removeAttribute("disabled"); userIn.style.opacity = "1"; }
        if (passIn) { passIn.removeAttribute("disabled"); passIn.style.opacity = "1"; }
    }
    checkInputValidation();
}

// ==========================================================================
// 7. Popup Dropdown Handlers
// ==========================================================================
function toggleLanguagePopup(anchor) {
    var popup = document.getElementById("popupLanguage");
    if (!popup) return;
    
    anchor.getBoundingClientRect();
    popup.style.width = anchor.offsetWidth + "px";
    anchor.popup(popup, "bottom-left");
}

function chooseLanguage(lang) {
    setLanguage(lang);
    saveSettingsToNative();
    var popup = document.getElementById("popupLanguage");
    if (popup) popup.state.popup = false;
}

function logDebug(msg) {
    console.log(msg);
    if (typeof Native_Log === "function") {
        try { Native_Log(msg); } catch (e) {}
    }
}

function getFirstAvailableDriveLetter() {
    var letters = [];
    if (typeof Native_GetAvailableDriveLetters === "function") {
        try {
            var raw = Native_GetAvailableDriveLetters();
            if (typeof raw === "string") letters = JSON.parse(raw) || [];
            else if (Array.isArray(raw)) letters = raw;
        } catch(e) {}
    }
    // Filter out Auto / *
    letters = (letters || []).filter(function(l) { return l && l !== "Auto" && l !== "*"; });
    if (letters.length === 0) {
        letters = ["Z:", "Y:", "X:", "W:", "V:", "U:", "T:", "S:", "R:", "Q:", "P:", "O:", "N:", "M:", "L:", "K:", "J:", "I:", "H:", "G:", "F:", "E:", "D:"];
    }

    // Exclude currently occupied letters by connected mountedDrives
    var usedLetters = {};
    for (var i = 0; i < mountedDrives.length; i++) {
        var md = mountedDrives[i];
        if (md.status === "connected" && md.currentMountedLetter) {
            var ltr = md.currentMountedLetter.toUpperCase();
            if (ltr.length === 1) ltr += ":";
            usedLetters[ltr] = true;
        }
    }

    for (var j = 0; j < letters.length; j++) {
        var candidate = letters[j].toUpperCase();
        if (candidate.length === 1) candidate += ":";
        if (!usedLetters[candidate]) {
            return candidate;
        }
    }
    return letters[0] || "Z:";
}

function loadDrivesFromNative() {
    logDebug("loadDrivesFromNative called, typeof Native_GetDrivesList=" + typeof Native_GetDrivesList);
    if (typeof Native_GetDrivesList === "function") {
        try {
            var raw = Native_GetDrivesList();
            logDebug("Native_GetDrivesList raw output: " + raw);
            if (typeof raw === "string") {
                mountedDrives = JSON.parse(raw) || [];
            } else if (Array.isArray(raw)) {
                mountedDrives = raw;
            }
            logDebug("parsed mountedDrives length=" + mountedDrives.length);
        } catch (e) {
            logDebug("Native_GetDrivesList exception: " + e);
        }
    }
    
    refreshDriveListView();
    updateAllDrivesSpace();
}

function loadDriveLettersFromNative() {
    var letters = [];
    if (typeof Native_GetAvailableDriveLetters === "function") {
        try {
            var raw = Native_GetAvailableDriveLetters();
            if (typeof raw === "string") {
                letters = JSON.parse(raw) || [];
            } else if (Array.isArray(raw)) {
                letters = raw;
            }
        } catch (e) {}
    }
    if (!letters || letters.length === 0) {
        letters = ["Auto", "Z:", "Y:", "X:", "W:", "V:", "U:", "T:", "S:", "R:", "Q:", "P:", "O:", "N:", "M:", "L:", "K:", "J:", "I:", "H:", "G:", "F:", "E:", "D:"];
    }
    if (letters.indexOf("Auto") < 0) {
        letters.unshift("Auto");
    }

    var popup = document.getElementById("driveLetterPopup");
    if (popup) {
        var html = "";
        for (var i = 0; i < letters.length; i++) {
            html += '<div class="option-item" data-value="' + letters[i] + '">' + letters[i] + '</div>';
        }
        popup.innerHTML = html;
    }

    if (!selectedDriveLetter) {
        selectedDriveLetter = letters[0] || "Auto";
    }
    var valEl = document.getElementById("driveLetterVal");
    if (valEl) valEl.innerText = selectedDriveLetter;
}

async function refreshMountedLetters() {
    for (let i = 0; i < mountedDrives.length; i++) {
        let drive = mountedDrives[i];
        if (drive.status === "connected") {
            let realLetter = "";
            if (typeof Native_GetMountedDriveLetter === "function") {
                try { realLetter = Native_GetMountedDriveLetter(drive.name); } catch(e) {}
            }
            if (realLetter) {
                drive.currentMountedLetter = realLetter;
            } else if (!drive.currentMountedLetter || drive.currentMountedLetter === "*" || drive.currentMountedLetter === "Auto") {
                if (drive.letter !== "*" && drive.letter !== "Auto") {
                    drive.currentMountedLetter = drive.letter;
                }
            }
        } else {
            drive.currentMountedLetter = "";
        }
    }
}

function onNativeGetDriveSpaceResponse(letter, resStr) {
    try {
        var space = typeof resStr === "string" ? JSON.parse(resStr) : resStr;
        if (space && space.success) {
            for (var i = 0; i < mountedDrives.length; i++) {
                var d = mountedDrives[i];
                var currentLtr = d.currentMountedLetter || d.letter;
                if (d.letter === letter || currentLtr === letter || (currentLtr && currentLtr.toUpperCase().replace("\\", "") === letter.toUpperCase().replace("\\", ""))) {
                    d.spaceTotalGB = space.totalGB.toFixed(1) + " GB";
                    d.spaceFreeGB = space.freeGB.toFixed(1) + " GB";
                    d.spaceUsedGB = space.usedGB.toFixed(1) + " GB";
                    d.spacePercent = space.usedPercent;
                    break;
                }
            }
            renderDriveCards();
        }
    } catch (e) {}
}
globalThis.onNativeGetDriveSpaceResponse = onNativeGetDriveSpaceResponse;

function updateAllDrivesSpace() {
    for (var i = 0; i < mountedDrives.length; i++) {
        var d = mountedDrives[i];
        if (d.status === "connected") {
            if (!d.currentMountedLetter && typeof Native_GetMountedDriveLetter === "function") {
                try { d.currentMountedLetter = Native_GetMountedDriveLetter(d.name); } catch(e) {}
            }
            var letterToQuery = d.currentMountedLetter || d.letter;
            if (letterToQuery && letterToQuery !== "Auto" && letterToQuery !== "*") {
                if (typeof Native_GetDriveSpaceAsync === "function") {
                    try { Native_GetDriveSpaceAsync({ letter: letterToQuery }); } catch(e) {}
                } else if (typeof Native_GetDriveSpace === "function") {
                    try {
                        var raw = Native_GetDriveSpace(letterToQuery);
                        var space = typeof raw === "string" ? JSON.parse(raw) : raw;
                        if (space && space.success) {
                            d.spaceTotalGB = space.totalGB.toFixed(1) + " GB";
                            d.spaceFreeGB = space.freeGB.toFixed(1) + " GB";
                            d.spaceUsedGB = space.usedGB.toFixed(1) + " GB";
                            d.spacePercent = space.usedPercent;
                        }
                    } catch (e) {}
                }
            }
        }
    }
    renderDriveCards();
}

function refreshDriveListView() {
    logDebug("refreshDriveListView, mountedDrives.length=" + mountedDrives.length);
    var emptyWrap = document.getElementById("emptyStateWrap");
    var driveList = document.getElementById("driveListContainer");
    if (!driveList) return;
    
    if (mountedDrives.length === 0) {
        if (emptyWrap) emptyWrap.style.display = "flex";
        driveList.style.display = "none";
    } else {
        if (emptyWrap) emptyWrap.style.display = "none";
        driveList.style.display = "block";
        renderDriveCards();
    }
}

function renderDriveCards() {
    var driveList = document.getElementById("driveListContainer");
    if (!driveList) return;
    
    var isZh = currentLanguage === "zh";
    var html = "";
    for (var i = 0; i < mountedDrives.length; i++) {
        var d = mountedDrives[i];
        var isConn = d.status === "connected";
        var isConnecting = d.status === "connecting" || d.status === "disconnecting";
        var isDisconnecting = d.status === "disconnecting";
        var displayName = d.name || d.protocol || "Drive";

        if (isConn && (!d.currentMountedLetter || d.currentMountedLetter === "*" || d.currentMountedLetter === "Auto")) {
            if (typeof Native_GetMountedDriveLetter === "function") {
                try {
                    var ltr = Native_GetMountedDriveLetter(d.name);
                    if (ltr) d.currentMountedLetter = ltr;
                } catch(e) {}
            }
        }
        var hasRealLetter = isConn && d.currentMountedLetter && d.currentMountedLetter !== "*" && d.currentMountedLetter !== "Auto";
        var letterText = hasRealLetter ? d.currentMountedLetter : (d.letter === "*" ? "Auto" : d.letter);
        var titleText = displayName + (d.host ? " (" + d.host + ")" : "") + " (" + letterText + ")";
        
        var spaceText = "Unknown space";
        var percentWidth = 0;
        if (isConn) {
            if (d.spaceTotalGB) {
                spaceText = d.spaceFreeGB + (isZh ? " 可用 (共 " : " free of ") + d.spaceTotalGB + ")";
                percentWidth = d.spacePercent || 15;
            } else {
                spaceText = (isZh ? "已连接到 " : "Connected to ") + (hasRealLetter ? d.currentMountedLetter : letterText);
                percentWidth = 20;
            }
        } else if (isDisconnecting) {
            spaceText = isZh ? "正在断开连接..." : "Disconnecting...";
            percentWidth = 50;
        } else if (isConnecting) {
            spaceText = isZh ? "正在连接挂载..." : "Connecting...";
            percentWidth = 50;
        } else if (d.status === "error") {
            spaceText = (isZh ? "连接失败: " : "Connection Failed: ") + (d.errorMsg || (isZh ? "未知错误" : "Unknown Error"));
            percentWidth = 0;
        } else {
            spaceText = isZh ? "未连接" : "Disconnected";
            percentWidth = 0;
        }

        var titleOpen = isZh ? "在资源管理器中打开" : "Open in File Explorer";
        var titleDisconnect = isZh ? "断开连接" : "Disconnect";
        var titleConnect = isZh ? "连接挂载" : "Connect";
        var titleEdit = isZh ? "设置/编辑" : "Edit Settings";
        var titleDelete = isZh ? "删除" : "Delete";

        html += '<div class="drive-card ' + (isConn ? 'connected' : (isConnecting ? 'connecting' : (d.status === 'error' ? 'error' : 'disconnected'))) + '" data-index="' + i + '">' +
            '<div class="drive-card-left-icon">' +
                '<svg width="32" height="36" viewBox="0 0 24 24"><path fill="#ffffff" d="M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"/></svg>' +
            '</div>' +
            '<div class="drive-card-info">' +
                '<div class="drive-card-title">' + titleText + '</div>' +
                '<div class="drive-progress-bar"><div class="drive-progress-fill" style="width:' + percentWidth + '%;"></div></div>' +
                '<div class="drive-card-sub-row">' +
                    '<span class="drive-space-text ' + (d.status === 'error' ? 'text-error' : '') + '">' + spaceText + '</span>' +
                '</div>' +
            '</div>' +
            '<div class="drive-card-actions">' +
                (isConn ?
                    '<div class="btn-drive-circle btn-toggle-drive" data-index="' + i + '" onclick="toggleDriveStatus(' + i + ')" title="' + titleDisconnect + '">' +
                        '<svg width="34" height="34" viewBox="0 0 34 34" shape-rendering="geometricPrecision"><circle class="circle-stop" cx="17" cy="17" r="15.5" fill="#ffffff" stroke="transparent" stroke-width="1.2"/><rect x="11" y="11" width="12" height="12" fill="#181818" rx="0.8"/></svg>' +
                    '</div>' :
                    (isConnecting ?
                        '<div class="btn-drive-circle btn-toggle-drive is-connecting" data-index="' + i + '" title="' + (isDisconnecting ? (isZh ? "正在断开连接..." : "Disconnecting...") : (isZh ? "正在连接..." : "Connecting...")) + '">' +
                            '<svg class="win10-spinner-canvas" width="34" height="34" viewBox="0 0 34 34" shape-rendering="geometricPrecision">' +
                                (isDisconnecting ?
                                    '<rect x="11" y="11" width="12" height="12" fill="#ffffff" rx="0.8"/>' :
                                    '<path fill="#ffffff" d="M12 9v16l13-8z"/>'
                                ) +
                                '<g class="win10-progress-ring">' +
                                    '<circle class="wdot wdot-0" cx="30.1" cy="23.1" r="1.8" fill="#a8a8a8" opacity="0"/>' +
                                    '<circle class="wdot wdot-1" cx="30.1" cy="23.1" r="1.8" fill="#a8a8a8" opacity="0"/>' +
                                    '<circle class="wdot wdot-2" cx="30.1" cy="23.1" r="1.8" fill="#a8a8a8" opacity="0"/>' +
                                    '<circle class="wdot wdot-3" cx="30.1" cy="23.1" r="1.8" fill="#a8a8a8" opacity="0"/>' +
                                    '<circle class="wdot wdot-4" cx="30.1" cy="23.1" r="1.8" fill="#a8a8a8" opacity="0"/>' +
                                '</g>' +
                            '</svg>' +
                        '</div>' :
                        '<div class="btn-drive-circle btn-toggle-drive" data-index="' + i + '" onclick="toggleDriveStatus(' + i + ')" title="' + titleConnect + '">' +
                            '<svg width="34" height="34" viewBox="0 0 34 34" shape-rendering="geometricPrecision"><circle class="circle-bg" cx="17" cy="17" r="15.5" fill="transparent" stroke="transparent" stroke-width="1.2"/><path fill="#ffffff" d="M12 9v16l13-8z"/></svg>' +
                        '</div>'
                    )
                ) +
                '<div class="btn-drive-circle btn-edit-drive" data-index="' + i + '" onclick="editDrive(' + i + ')" title="' + titleEdit + '">' +
                    '<svg width="34" height="34" viewBox="0 0 34 34" shape-rendering="geometricPrecision"><circle class="circle-bg" cx="17" cy="17" r="15.5" fill="#333333" stroke="#ffffff" stroke-width="1.2"/><g transform="translate(8, 8) scale(0.75)"><path fill="#ffffff" d="M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6-3.6 3.6z"/></g></svg>' +
                '</div>' +
                '<div class="btn-drive-circle btn-delete-drive" data-index="' + i + '" onclick="deleteDrive(' + i + ')" title="' + titleDelete + '">' +
                    '<svg width="34" height="34" viewBox="0 0 34 34" shape-rendering="geometricPrecision"><circle class="circle-bg" cx="17" cy="17" r="15.5" fill="#333333" stroke="#ffffff" stroke-width="1.2"/><g transform="translate(8, 8) scale(0.75)"><path fill="#ffffff" d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/></g></svg>' +
                '</div>' +
            '</div>' +
        '</div>';
    }
    driveList.innerHTML = html;
    // Draw the button state on the clickable container. This avoids relying on
    // Sciter's inconsistent :hover matching for SVG descendants.
    var circleBackgrounds = driveList.querySelectorAll(".btn-edit-drive circle.circle-bg, .btn-delete-drive circle.circle-bg, .btn-toggle-drive:not(.is-connecting) circle.circle-bg");
    for (var circleIndex = 0; circleIndex < circleBackgrounds.length; circleIndex++) {
        circleBackgrounds[circleIndex].setAttribute("fill", "#333333");
        circleBackgrounds[circleIndex].setAttribute("stroke", "#777777");
    }
    var stopCircles = driveList.querySelectorAll("circle.circle-stop");
    for (var stopIndex = 0; stopIndex < stopCircles.length; stopIndex++) {
        stopCircles[stopIndex].setAttribute("fill", "#333333");
        stopCircles[stopIndex].setAttribute("stroke", "#777777");
    }
    var stopSquares = driveList.querySelectorAll("circle.circle-stop + rect");
    for (var squareIndex = 0; squareIndex < stopSquares.length; squareIndex++) {
        stopSquares[squareIndex].setAttribute("fill", "#ffffff");
    }
}

function getUniqueDriveName(baseName, excludeName) {
    if (!baseName) baseName = "Drive";
    var name = baseName;
    var counter = 2;
    var exists = function(n) {
        for (var i = 0; i < mountedDrives.length; i++) {
            if (mountedDrives[i].name === n && n !== excludeName) {
                return true;
            }
        }
        return false;
    };
    while (exists(name)) {
        name = baseName + " " + counter;
        counter++;
    }
    return name;
}

function selectProtocol(proto, updateName) {
    if (updateName === undefined) updateName = true;
    currentProtocol = proto || "FTP";

    var cards = document.querySelectorAll(".protocol-card");
    for (var i = 0; i < cards.length; i++) {
        if (cards[i].getAttribute("data-protocol") === currentProtocol) {
            cards[i].classList.add("active");
        } else {
            cards[i].classList.remove("active");
        }
    }

    var portInput = document.getElementById("inputPort");
    var lblProtocol = document.getElementById("lblProtocol");
    var rowPrivateKey = document.getElementById("rowPrivateKey");
    var rowFtpOptions = document.getElementById("rowFtpOptions");
    var driveNameInput = document.getElementById("inputDriveName");

    if (currentProtocol === "SFTP") {
        if (portInput) portInput.value = "22";
        if (lblProtocol) lblProtocol.innerText = "sftp://";
        if (rowPrivateKey) rowPrivateKey.style.display = "flex";
        if (rowFtpOptions) rowFtpOptions.style.display = "none";
    } else if (currentProtocol === "FTP") {
        if (portInput) portInput.value = "21";
        if (lblProtocol) lblProtocol.innerText = "ftp://";
        if (rowPrivateKey) rowPrivateKey.style.display = "none";
        if (rowFtpOptions) rowFtpOptions.style.display = "flex";
    } else { // WebDAV
        if (portInput) portInput.value = "443";
        if (lblProtocol) lblProtocol.innerText = "https://";
        if (rowPrivateKey) rowPrivateKey.style.display = "none";
        if (rowFtpOptions) rowFtpOptions.style.display = "none";
    }

    if (updateName && driveNameInput && !editingDriveOriginalName) {
        driveNameInput.value = getUniqueDriveName(currentProtocol, "");
    }
    checkInputValidation();
}

var editingDriveOriginalName = "";
var editingOriginalPassword = "";

function openNewDrivePanel() {
    editingDriveOriginalName = "";
    editingOriginalPassword = "";
    var titleEl = document.querySelector("#viewNewDrive .page-title");
    if (titleEl) titleEl.innerText = currentLanguage === "zh" ? "新建驱动器" : "New Drive";

    selectedDriveLetter = "Auto";
    var driveLetterValEl = document.getElementById("driveLetterVal");
    if (driveLetterValEl) driveLetterValEl.innerText = "Auto";

    selectProtocol("FTP");
    loadDriveLettersFromNative();

    var driveNameInput = document.getElementById("inputDriveName");
    if (driveNameInput) driveNameInput.value = getUniqueDriveName("FTP", "");

    var hostInput = document.getElementById("inputHost");
    if (hostInput) hostInput.value = "";

    var portInput = document.getElementById("inputPort");
    if (portInput) portInput.value = "21";

    var pathInput = document.getElementById("inputPath");
    if (pathInput) pathInput.value = "";

    var userInput = document.getElementById("inputUsername");
    if (userInput) userInput.value = "";

    var passInput = document.getElementById("inputPassword");
    if (passInput) passInput.value = "";

    var anonChk = document.getElementById("chkAnonymous");
    if (anonChk) { anonChk.checked = false; onAnonymousChange(anonChk); }

    var sslChk = document.getElementById("chkSSL");
    if (sslChk) { sslChk.checked = false; onSSLChange(sslChk); }

    var recChk = document.getElementById("chkReconnect");
    if (recChk) recChk.checked = true;

    var localChk = document.getElementById("chkLocalDisk");
    if (localChk) localChk.checked = false;

    var roChk = document.getElementById("chkReadOnly");
    if (roChk) roChk.checked = false;

    var lockChk = document.getElementById("chkFileLock");
    if (lockChk) lockChk.checked = false;

    var passvChk = document.getElementById("chkPassive");
    if (passvChk) passvChk.checked = true;

    var explChk = document.getElementById("chkExplicit");
    if (explChk) explChk.checked = false;

    slideIn("viewNewDrive");
    checkInputValidation();
}

function editDrive(index) {
    if (!mountedDrives[index]) return;
    var d = mountedDrives[index];
    editingDriveOriginalName = d.name;
    editingOriginalPassword = d.password || "";

    selectProtocol(d.protocol || "WebDAV", false);
    selectedDriveLetter = d.letter || "Auto";
    var letterValEl = document.getElementById("driveLetterVal");
    if (letterValEl) letterValEl.innerText = selectedDriveLetter;

    var driveNameInput = document.getElementById("inputDriveName");
    if (driveNameInput) driveNameInput.value = d.name || "";

    var hostInput = document.getElementById("inputHost");
    if (hostInput) hostInput.value = d.host || "";

    var portInput = document.getElementById("inputPort");
    if (portInput) portInput.value = d.port || "";

    var pathInput = document.getElementById("inputPath");
    if (pathInput) pathInput.value = (d.path && d.path !== "undefined") ? d.path : "";

    var userInput = document.getElementById("inputUsername");
    if (userInput) userInput.value = d.username || "";

    var passInput = document.getElementById("inputPassword");
    if (passInput) passInput.value = d.password || "";

    var anonChk = document.getElementById("chkAnonymous");
    if (anonChk) {
        anonChk.checked = !!d.isAnonymous;
        onAnonymousChange(anonChk);
    }

    var sslChk = document.getElementById("chkSSL");
    if (sslChk) {
        sslChk.checked = !!d.isSSL;
        onSSLChange(sslChk);
    }

    var recChk = document.getElementById("chkReconnect");
    if (recChk) recChk.checked = !!d.isReconnect;

    var localChk = document.getElementById("chkLocalDisk");
    if (localChk) localChk.checked = !!d.isLocalDisk;

    var roChk = document.getElementById("chkReadOnly");
    if (roChk) roChk.checked = !!d.isReadOnly;

    var lockChk = document.getElementById("chkFileLock");
    if (lockChk) lockChk.checked = !!d.isFileLock;

    var privKeyInput = document.getElementById("inputPrivateKey");
    if (privKeyInput) privKeyInput.value = d.privateKey || "";

    var charsetVal = document.getElementById("charsetVal");
    if (charsetVal) charsetVal.innerText = d.charset || "utf-8";

    var passvChk = document.getElementById("chkPassive");
    if (passvChk) passvChk.checked = d.isPassive !== false;

    var explChk = document.getElementById("chkExplicit");
    if (explChk) explChk.checked = !!d.isExplicit;

    var titleEl = document.querySelector("#viewNewDrive .page-title");
    if (titleEl) titleEl.innerText = (currentLanguage === "zh" ? "编辑驱动器" : "Edit Drive") + " (" + d.name + ")";

    slideIn("viewNewDrive");
    checkInputValidation();
}

async function openDriveLetter(index) {
    if (mountedDrives[index]) {
        var d = mountedDrives[index];
        if (d.status === "connected") {
            var letterToOpen = d.currentMountedLetter || d.letter;
            if (!letterToOpen || letterToOpen === "Auto" || letterToOpen === "*") {
                await refreshMountedLetters();
                letterToOpen = d.currentMountedLetter || d.letter;
            }
            if (letterToOpen && letterToOpen !== "Auto" && letterToOpen !== "*") {
                if (typeof Native_OpenDrive === "function") {
                    Native_OpenDrive(letterToOpen);
                }
            }
        }
    }
}

function checkAndPromptWinFsp() {
    if (typeof Native_CheckWinFsp === "function") {
        try {
            var installed = Native_CheckWinFsp();
            if (!installed) {
                if (typeof Native_InstallWinFsp === "function") {
                    Native_InstallWinFsp();
                }
                showErrorModal(
                    currentLanguage === "zh" ? "WinFsp 驱动缺失" : "WinFsp Driver Missing",
                    currentLanguage === "zh" ? "系统未检测到 WinFsp 组件，已自动为您调起 WinFsp 安装程序，请完成安装后重新连接。" : "WinFsp is not installed. The installer has been launched for you."
                );
                return false;
            }
        } catch (e) {}
    }
    return true;
}

function showErrorModal(title, message) {
    // 错误信息已直接呈现于卡片下方的红色提示文本中，无需弹窗阻断
}

function closeAppErrorModal() {
    var modal = document.getElementById("appErrorModal");
    if (modal) modal.classList.remove("active");
}

function resetProgressRingAnimation() {
    var existingRings = document.querySelectorAll(".win10-progress-ring");
    if (!existingRings || existingRings.length === 0) {
        ringStartTime = Date.now();
    }
}

function toggleDriveStatus(index) {
    logDebug("[toggleDriveStatus] Entering, index=" + index);
    if (!mountedDrives[index]) {
        logDebug("[toggleDriveStatus] Drive not found at index=" + index);
        return;
    }
    var d = mountedDrives[index];
    logDebug("[toggleDriveStatus] Drive status=" + d.status + ", name=" + d.name);
    if (d.status === "connecting" || d.status === "disconnecting") return;
    
    (async function() {
        if (d.status === "connected") {
            d.status = "disconnecting";
            d.errorMsg = "";
            resetProgressRingAnimation();
            renderDriveCards();
            
            logDebug("[toggleDriveStatus] Triggering HTTP unmount for: " + d.name);
            let [res] = await Promise.all([
                unmountDriveHTTP(d.name),
                new Promise(r => setTimeout(r, 400)) // smooth visual animation transition
            ]);
            logDebug("[toggleDriveStatus] HTTP unmount response: " + JSON.stringify(res));
            if (res.success) {
                d.status = "disconnected";
                d.errorMsg = "";
                renderDriveCards();
            } else {
                d.status = "connected"; // revert
                d.errorMsg = res.error;
                renderDriveCards();
                showErrorModal(d.name + (currentLanguage === "zh" ? " 断开连接失败" : " Disconnect Failed"), res.error);
            }
        } else {
            if (!checkAndPromptWinFsp()) {
                d.status = "error";
                d.errorMsg = "系统未安装 WinFsp 驱动组件，已自动调起安装程序，请安装后重新连接";
                renderDriveCards();
                return;
            }
            d.status = "connecting";
            d.errorMsg = "";
            resetProgressRingAnimation();
            renderDriveCards();
            
            logDebug("[toggleDriveStatus] Triggering HTTP mount for: " + d.name);
            let [res] = await Promise.all([
                mountDriveHTTP(d),
                new Promise(r => setTimeout(r, 400)) // smooth visual animation transition
            ]);
            logDebug("[toggleDriveStatus] HTTP mount response: " + JSON.stringify(res));
            if (res.success) {
                d.status = "connected";
                d.errorMsg = "";
                await refreshMountedLetters();
                renderDriveCards();
                updateAllDrivesSpace();
                // Automatically open explorer if set
                let openExp = false;
                if (typeof Native_GetSettings === "function") {
                    try {
                        let raw = Native_GetSettings();
                        let s = typeof raw === "string" ? JSON.parse(raw) : raw;
                        if (s && s.openExplorerOnConnect) openExp = true;
                    } catch(e) {}
                }
                let letterToOpen = d.currentMountedLetter || d.letter;
                if (openExp && typeof Native_OpenDrive === "function" && letterToOpen && letterToOpen !== "Auto") {
                    Native_OpenDrive(letterToOpen);
                }
            } else {
                d.status = "error";
                d.errorMsg = res.error;
                renderDriveCards();
                showErrorModal(d.name + (currentLanguage === "zh" ? " 连接失败" : " Connection Failed"), res.error);
            }
        }
    })();
}

var deletePendingIndex = -1;

function deleteDrive(index) {
    if (!mountedDrives[index]) return;
    deletePendingIndex = index;
    
    var d = mountedDrives[index];
    var modal = document.getElementById("confirmDeleteModal");
    var titleEl = document.getElementById("confirmDeleteModalTitle");
    var bodyEl = document.getElementById("confirmDeleteModalBody");
    var btnOk = document.getElementById("btnConfirmDeleteOk");
    var btnCancel = document.getElementById("btnConfirmDeleteCancel");

    var isZh = (currentLanguage === "zh");
    if (titleEl) {
        titleEl.innerText = isZh ? "删除驱动器确认" : "Confirm Delete";
    }
    if (bodyEl) {
        bodyEl.innerText = isZh ? 
            "您确定要删除网络驱动器 \"" + d.name + "\" 吗？此操作不可撤销。" : 
            "Are you sure you want to delete network drive \"" + d.name + "\"? This action cannot be undone.";
    }
    if (btnOk) {
        btnOk.innerText = isZh ? "确定" : "OK";
    }
    if (btnCancel) {
        btnCancel.innerText = isZh ? "取消" : "Cancel";
    }
    if (modal) {
        modal.classList.add("active");
    }
}

function closeConfirmDeleteModal() {
    var modal = document.getElementById("confirmDeleteModal");
    if (modal) modal.classList.remove("active");
    deletePendingIndex = -1;
}

async function confirmDeleteDrive() {
    var index = deletePendingIndex;
    closeConfirmDeleteModal();
    if (index === -1 || !mountedDrives[index]) return;
    var d = mountedDrives[index];
    
    // 如果当前处于挂载连接状态，先执行停止与卸载
    if (d.status === "connected" || d.status === "connecting" || d.status === "disconnecting") {
        try {
            await unmountDriveHTTP(d.name);
        } catch (e) {
            console.error("unmountDriveHTTP on delete error:", e);
        }
    }

    if (typeof Native_DeleteDrive === "function") {
        try {
            Native_DeleteDrive(d.name);
        } catch (e) {
            console.error("Native_DeleteDrive error", e);
        }
    }
    try {
        await callRcloneRC("config/delete", { name: d.name });
    } catch (e) {}
    
    // 重新通过名称定位索引并安全移除，避免异步等待期间索引漂移
    var curIdx = mountedDrives.findIndex(function(item) { return item.name === d.name; });
    if (curIdx >= 0) {
        mountedDrives.splice(curIdx, 1);
    }
    refreshDriveListView();
    loadDriveLettersFromNative();
}

function saveAndMountDrive() {
    try {
        var hostInput = document.getElementById("inputHost");
        var driveNameInput = document.getElementById("inputDriveName");
        var portInput = document.getElementById("inputPort");
        var pathInput = document.getElementById("inputPath");
        var userInput = document.getElementById("inputUsername");
        var passInput = document.getElementById("inputPassword");
        var anonChk = document.getElementById("chkAnonymous");
        var sslCheckbox = document.getElementById("chkSSL");
        var reconnectChk = document.getElementById("chkReconnect");
        var localDiskChk = document.getElementById("chkLocalDisk");
        var readOnlyChk = document.getElementById("chkReadOnly");
        var fileLockChk = document.getElementById("chkFileLock");
        var privKeyInput = document.getElementById("inputPrivateKey");
        var charsetEl = document.getElementById("charsetVal");
        var passiveChk = document.getElementById("chkPassive");
        var explicitChk = document.getElementById("chkExplicit");

        var host = hostInput ? (hostInput.value || "").trim() : "";
        if (!host) {
            if (hostInput) hostInput.focus();
            checkInputValidation();
            return;
        }

        var driveName = driveNameInput ? (driveNameInput.value || "").trim() : "";
        if (!driveName) driveName = currentProtocol || "WebDAV";

        var isAnon = anonChk ? !!anonChk.checked : false;
        var username = userInput ? (userInput.value || "").trim() : "";
        var password = passInput ? (passInput.value || "") : "";

        var portVal = portInput ? (portInput.value || "").trim() : (currentProtocol === "SFTP" ? "22" : (currentProtocol === "FTP" ? "21" : "443"));

        var isReconnect = reconnectChk ? !!reconnectChk.checked : true;

        var origName = editingDriveOriginalName;
        var origPass = editingOriginalPassword;
        var isPasswordModified = (!origName) || (password !== origPass);
        editingDriveOriginalName = "";
        editingOriginalPassword = "";
        var wasConnected = false;
        var existingIdx = -1;

        var origLetter = "";
        if (origName) {
            for (var k = 0; k < mountedDrives.length; k++) {
                if (mountedDrives[k].name === origName) {
                    existingIdx = k;
                    origLetter = mountedDrives[k].letter || "";
                    if (mountedDrives[k].status === "connected" || mountedDrives[k].status === "connecting" || mountedDrives[k].status === "disconnecting") {
                        wasConnected = true;
                    }
                    break;
                }
            }
            if (driveName !== origName) {
                driveName = getUniqueDriveName(driveName, origName);
            }
        } else {
            driveName = getUniqueDriveName(driveName, "");
        }

        // 是否需要触发连接：若当前本身已经连接，则确定后必须真实重连；若是新建驱动器，则根据是否勾选登录时重连
        var shouldMount = wasConnected ? true : (!origName && !!isReconnect);

        var driveObj = {
            name: driveName,
            protocol: currentProtocol || "WebDAV",
            letter: selectedDriveLetter || "Auto",
            host: host,
            port: portVal,
            path: pathInput ? (pathInput.value || "").trim() : "",
            username: username,
            password: password,
            isAnonymous: isAnon,
            isSSL: sslCheckbox ? !!sslCheckbox.checked : (currentProtocol === "WebDAV" ? true : false),
            isReconnect: isReconnect,
            isLocalDisk: localDiskChk ? !!localDiskChk.checked : false,
            isReadOnly: readOnlyChk ? !!readOnlyChk.checked : false,
            isFileLock: fileLockChk ? !!fileLockChk.checked : false,
            privateKey: privKeyInput ? (privKeyInput.value || "").trim() : "",
            charset: charsetEl ? (charsetEl.innerText || "utf-8") : "utf-8",
            isPassive: passiveChk ? !!passiveChk.checked : true,
            isExplicit: explicitChk ? !!explicitChk.checked : false,
            status: shouldMount ? "connecting" : "disconnected"
        };

        // 1. 立即同步到前端内存数组并刷新视图
        if (existingIdx >= 0) {
            mountedDrives[existingIdx] = driveObj;
        } else {
            mountedDrives.push(driveObj);
        }

        if (shouldMount) {
            resetProgressRingAnimation();
        }
        refreshDriveListView();

        // 2. 原生层保存持久化
        if (typeof Native_SaveDrive === "function") {
            try {
                Native_SaveDrive(driveObj);
            } catch (err) {
                console.error("Native_SaveDrive exception:", err);
            }
        }

        // 3. 关闭抽屉，刷新可用盘符
        slideOut("viewNewDrive");
        loadDriveLettersFromNative();

        // 4. 异步处理旧挂载卸载、配置同步与重新挂载
        setTimeout(async function() {
            try {
                // 如果之前处于连接状态，先真实断开旧连接
                if (wasConnected) {
                    if (origName) {
                        try { await unmountDriveHTTP(origName); } catch(e) {}
                    }
                    if (origLetter && origLetter !== "Auto" && origLetter !== "*") {
                        try { await callRcloneRC("mount/unmount", { mountPoint: origLetter }); } catch(e) {}
                    }
                    if (driveObj.letter && driveObj.letter !== origLetter && driveObj.letter !== "Auto" && driveObj.letter !== "*") {
                        try { await callRcloneRC("mount/unmount", { mountPoint: driveObj.letter }); } catch(e) {}
                    }
                }

                // 如果改名了，删除旧配置
                if (origName && origName !== driveName) {
                    if (typeof Native_DeleteDrive === "function") {
                        try { Native_DeleteDrive(origName); } catch (e) {}
                    }
                    try {
                        await callRcloneRC("config/delete", { name: origName });
                    } catch (e) {}
                }

                // 同步新配置到 daemon
                await syncDriveConfigToRC(driveObj, isPasswordModified);

                // 触发挂载
                if (shouldMount) {
                    if (!checkAndPromptWinFsp()) {
                        driveObj.status = "error";
                        driveObj.errorMsg = "系统未安装 WinFsp 驱动组件，已自动调起安装程序，请安装后重新连接";
                        renderDriveCards();
                        return;
                    }
                    driveObj.status = "connecting";
                    driveObj.errorMsg = "";
                    renderDriveCards();

                    let res = await mountDriveHTTP(driveObj);
                    if (res.success) {
                        driveObj.status = "connected";
                        driveObj.errorMsg = "";
                        await refreshMountedLetters();
                        renderDriveCards();
                        updateAllDrivesSpace();
                        // Open explorer if set
                        let openExp = false;
                        if (typeof Native_GetSettings === "function") {
                            try {
                                let raw = Native_GetSettings();
                                let s = typeof raw === "string" ? JSON.parse(raw) : raw;
                                if (s && s.openExplorerOnConnect) openExp = true;
                            } catch(e) {}
                        }
                        let letterToOpen = driveObj.currentMountedLetter || driveObj.letter;
                        if (openExp && typeof Native_OpenDrive === "function" && letterToOpen && letterToOpen !== "Auto") {
                            Native_OpenDrive(letterToOpen);
                        }
                    } else {
                        driveObj.status = "error";
                        driveObj.errorMsg = res.error;
                        renderDriveCards();
                        showErrorModal(driveObj.name + (currentLanguage === "zh" ? " 连接失败" : " Connection Failed"), res.error);
                    }
                } else {
                    driveObj.status = "disconnected";
                    driveObj.errorMsg = "";
                    renderDriveCards();
                }
            } catch (err) {
                console.error("saveAndMountDrive async processing error:", err);
            }
        }, 20);
    } catch (e) {
        console.error("saveAndMountDrive error:", e);
    }
}

function onNativeDriveStatusChange(name, status, errMsg) {
    logDebug("onNativeDriveStatusChange: name=" + name + ", status=" + status + ", errMsg=" + errMsg);
    if (status === "connecting" || status === "disconnecting") {
        resetProgressRingAnimation();
    }
    for (var i = 0; i < mountedDrives.length; i++) {
        if (mountedDrives[i].name === name) {
            mountedDrives[i].status = status;
            mountedDrives[i].errorMsg = (status === "error") ? (errMsg || "") : "";
            if (status !== "connected") {
                mountedDrives[i].currentMountedLetter = "";
            }
            break;
        }
    }
    renderDriveCards();

    if (status === "connected") {
        refreshMountedLetters().then(() => {
            renderDriveCards();
            updateAllDrivesSpace();
        });
    } else if (status === "error" && errMsg) {
        showErrorModal(name + (currentLanguage === "zh" ? " 连接失败" : " Connection Failed"), errMsg);
    }

    if (errMsg && (errMsg.indexOf("WinFsp") >= 0 || errMsg.indexOf("winfsp") >= 0 || errMsg.indexOf("fuse") >= 0 || errMsg.indexOf("mount fail") >= 0)) {
        if (typeof Native_InstallWinFsp === "function") {
            try { Native_InstallWinFsp(); } catch (e) {}
        }
    }
}

function loadSettingsFromNative() {
    if (typeof Native_GetSettings !== "function") return;
    try {
        var raw = Native_GetSettings();
        var s = typeof raw === "string" ? JSON.parse(raw) : raw;
        if (s) {
            var rGUI = document.getElementById("radioAutoStartGUI");
            var rNoGUI = document.getElementById("radioAutoStartNoGUI");
            var chkExp = document.getElementById("chkOpenExplorer");
            var cacheIn = document.getElementById("inputCachePath");
            if (rGUI) rGUI.checked = !!s.autoStart;
            if (rNoGUI) rNoGUI.checked = !!s.autoStartNoGUI;
            if (chkExp) chkExp.checked = !!s.openExplorerOnConnect;
            if (cacheIn && s.cachePath) cacheIn.value = s.cachePath;
            if (s.language) setLanguage(s.language);
        }
    } catch (e) {}
}

function saveSettingsToNative() {
    if (typeof Native_SaveSettings !== "function") return;
    try {
        var rGUI = document.getElementById("radioAutoStartGUI");
        var rNoGUI = document.getElementById("radioAutoStartNoGUI");
        var chkExp = document.getElementById("chkOpenExplorer");
        var cacheIn = document.getElementById("inputCachePath");
        var obj = {
            autoStart: rGUI ? !!rGUI.checked : false,
            autoStartNoGUI: rNoGUI ? !!rNoGUI.checked : false,
            openExplorerOnConnect: chkExp ? !!chkExp.checked : false,
            language: currentLanguage,
            cachePath: cacheIn ? cacheIn.value : "C:\\ProgramData\\rcloneGUI\\Cache"
        };
        Native_SaveSettings(obj);
        // 同步到已运行的 rclone daemon；已挂载驱动器需重新挂载后才会切换缓存目录。
        try { callRcloneRC("options/set", { main: { CacheDir: obj.cachePath || "C:\\ProgramData\\rcloneGUI\\Cache" } }); } catch (e) {}
    } catch (e) {}
}

// Global exposure
globalThis.setLanguage = setLanguage;
globalThis.slideIn = slideIn;
globalThis.slideOut = slideOut;
globalThis.checkInputValidation = checkInputValidation;
globalThis.selectProtocol = selectProtocol;
globalThis.onSSLChange = onSSLChange;
globalThis.onAnonymousChange = onAnonymousChange;
globalThis.toggleLanguagePopup = toggleLanguagePopup;
globalThis.chooseLanguage = chooseLanguage;
globalThis.toggleDriveStatus = toggleDriveStatus;
globalThis.deleteDrive = deleteDrive;
globalThis.saveAndMountDrive = saveAndMountDrive;

async function updateAppVersions() {
    // 1. 获取 GUI 版本
    var guiVer = "";
    if (typeof Native_GetGuiVersion === "function") {
        try {
            guiVer = Native_GetGuiVersion() || "";
            if (guiVer) {
                var parts = guiVer.split(".");
                if (parts.length > 3) {
                    guiVer = parts.slice(0, 3).join(".");
                }
            }
        } catch(e) {}
    }
    var lblGui = document.getElementById("lblGuiVersion");
    if (lblGui) lblGui.innerText = guiVer;

    // 2. 访问远程 HTTP (Rclone RC core/version) 获取内核版本
    try {
        let res = await callRcloneRC("core/version", {});
        if (res && res.version) {
            let coreVer = res.version.replace(/^v/i, "");
            let lblCore = document.getElementById("lblCoreVersion");
            if (lblCore) lblCore.innerText = coreVer;
        }
    } catch (e) {
        logDebug("Failed to fetch rclone core version: " + e);
    }
}
globalThis.updateAppVersions = updateAppVersions;

var isAppInitialized = false;
async function initApp() {
    if (isAppInitialized) return;
    isAppInitialized = true;
    logDebug("initApp invoked (once)");

    await loadAllLanguages("zh");
    loadDriveLettersFromNative();
    loadSettingsFromNative();
    loadDrivesFromNative();
    updateAppVersions();

    // 轮询等待 Rclone 守护进程就绪并完成同步（最多重试 15 次，每次 300ms，确保首次启动绝不漏挂）
    setTimeout(async function waitForRcloneDaemonAndSync() {
        let maxRetries = 15;
        let isConnected = false;
        let listRes = null;

        for (let attempt = 1; attempt <= maxRetries; attempt++) {
            try {
                let res = await callRcloneRC("mount/listmounts", {});
                if (res && res.mountPoints !== undefined) {
                    logDebug("[HTTP Init] Rclone daemon is READY on attempt " + attempt);
                    listRes = res;
                    isConnected = true;
                    break;
                }
            } catch (e) {
                logDebug("[HTTP Init] Waiting for daemon, attempt " + attempt + ", error: " + e);
            }
            await new Promise(r => setTimeout(r, 300));
        }

        if (!isConnected || !listRes) {
            logDebug("[HTTP Init] Rclone daemon did not respond in time, reloading local drives list");
            loadDrivesFromNative();
            return;
        }

        // 重新从本地加载最新驱动器列表并比对挂载点
        loadDrivesFromNative();

        let mountPoints = listRes.mountPoints || [];
        logDebug("[HTTP Init] Query mounts returned: " + JSON.stringify(mountPoints));
        for (let i = 0; i < mountedDrives.length; i++) {
            let drive = mountedDrives[i];
            if (drive.letter === "*") drive.letter = "Auto";
            let matchedMp = mountPoints.find(mp => {
                let fsStr = (typeof mp === "object" ? (mp.Fs || mp.fs || "") : "");
                let rName = fsStr.split(":")[0].trim().toUpperCase();
                return rName === drive.name.toUpperCase();
            });
            if (matchedMp) {
                drive.status = "connected";
                let mpStr = typeof matchedMp === "string" ? matchedMp : (matchedMp.MountPoint || matchedMp.mountPoint || "");
                if (mpStr.endsWith("\\") || mpStr.endsWith("/")) mpStr = mpStr.slice(0, -1);
                drive.currentMountedLetter = mpStr;
            } else {
                drive.status = "disconnected";
                drive.currentMountedLetter = "";
            }
        }
        renderDriveCards();
        updateAllDrivesSpace();
        updateAppVersions();

        // 逐个顺序自动连接标记为“登录时重新连接”且当前未挂载的驱动器（避免并发抢占竞争）
        for (let i = 0; i < mountedDrives.length; i++) {
            let drive = mountedDrives[i];
            if (drive.isReconnect && drive.status !== "connected") {
                logDebug("[HTTP Init] Sequential auto reconnecting drive: " + drive.name);
                onNativeDriveStatusChange(drive.name, "connecting", "");
                let [mRes] = await Promise.all([
                    mountDriveHTTP(drive),
                    new Promise(r => setTimeout(r, 400)) // 确保动画平滑可见
                ]);
                if (mRes.success) {
                    onNativeDriveStatusChange(drive.name, "connected", "");
                } else {
                    onNativeDriveStatusChange(drive.name, "error", mRes.error);
                }
            }
        }
    }, 200);
}

var isRefreshingDrives = false;
async function manualRefreshDrives() {
    if (isRefreshingDrives) return;
    isRefreshingDrives = true;
    logDebug("[manualRefreshDrives] Triggered manual refresh...");

    var btn = document.getElementById("btnRefresh");
    if (btn) btn.classList.add("refreshing");

    try {
        // 1. 重新从本地 rclone.conf 加载驱动器配置列表（以 rclone.conf 为主）
        loadDrivesFromNative();
        loadDriveLettersFromNative();
        loadSettingsFromNative();

        // 2. 通过 HTTP 查询当前 rclone 守护进程 / 服务的实时挂载点状态
        let listRes = null;
        try {
            listRes = await callRcloneRC("mount/listmounts", {});
        } catch (e) {
            logDebug("[manualRefreshDrives] call mount/listmounts error: " + e);
        }

        let mountPoints = (listRes && listRes.mountPoints !== undefined) ? listRes.mountPoints : [];
        logDebug("[manualRefreshDrives] Active mounts: " + JSON.stringify(mountPoints));

        // 3. 将本地配置列表与远程实时挂载点进行匹配
        for (let i = 0; i < mountedDrives.length; i++) {
            let drive = mountedDrives[i];
            if (drive.letter === "*") drive.letter = "Auto";

            let matchedMp = mountPoints.find(mp => {
                let fsStr = typeof mp === "object" ? (mp.Fs || mp.fs || "") : (typeof mp === "string" ? mp : "");
                let rName = fsStr.split(":")[0].trim().toUpperCase();
                return rName === drive.name.toUpperCase();
            });

            if (matchedMp) {
                drive.status = "connected";
                let mpStr = typeof matchedMp === "string" ? matchedMp : (matchedMp.MountPoint || matchedMp.mountPoint || "");
                if (mpStr.endsWith("\\") || mpStr.endsWith("/")) mpStr = mpStr.slice(0, -1);
                drive.currentMountedLetter = mpStr;
            } else {
                drive.status = "disconnected";
                drive.currentMountedLetter = "";
            }
        }

        // 4. 刷新渲染列表视图并更新容量、版本与服务状态
        renderDriveCards();
        updateAllDrivesSpace();
        updateAppVersions();
        updateServiceStatus();
    } catch (err) {
        console.error("manualRefreshDrives error:", err);
    } finally {
        setTimeout(function() {
            if (btn) btn.classList.remove("refreshing");
            isRefreshingDrives = false;
        }, 500);
    }
}

globalThis.initApp = initApp;
globalThis.manualRefreshDrives = manualRefreshDrives;
globalThis.openDriveLetter = openDriveLetter;
globalThis.onNativeDriveStatusChange = onNativeDriveStatusChange;
globalThis.loadDrivesFromNative = loadDrivesFromNative;
globalThis.loadDriveLettersFromNative = loadDriveLettersFromNative;
globalThis.loadSettingsFromNative = loadSettingsFromNative;
globalThis.saveSettingsToNative = saveSettingsToNative;
globalThis.selectProtocol = selectProtocol;
globalThis.deleteDrive = deleteDrive;
globalThis.confirmDeleteDrive = confirmDeleteDrive;
globalThis.closeConfirmDeleteModal = closeConfirmDeleteModal;

// ==========================================================================
// 9. Initialization & Sciter Event Delegation
// ==========================================================================
document.on("ready", initApp);

document.on("click", "#btnConfirmDeleteOk", function() {
    confirmDeleteDrive();
});

document.on("click", "#btnConfirmDeleteCancel", function() {
    closeConfirmDeleteModal();
});

document.on("click", "#btnRefresh", function() {
    manualRefreshDrives();
});

document.on("click", "#btnAdd", function() {
    openNewDrivePanel();
});

    // 双击驱动器卡片在资源管理器中打开
    document.on("dblclick", ".drive-card", function(evt, card) {
        var idx = parseInt(card.getAttribute("data-index"), 10);
        if (!isNaN(idx)) {
            openDriveLetter(idx);
        }
    });

    document.on("click", "#btnSettings", function() {
        updateAppVersions();
        slideIn("viewSettings");
    });

    document.on("click", "#btnBackNewDrive", function() {
        slideOut("viewNewDrive");
    });

    document.on("click", "#btnCancelNewDrive", function() {
        slideOut("viewNewDrive");
    });

    document.on("click", "#btnBackSettings", function() {
        slideOut("viewSettings");
    });

    document.on("click", "#btnOkNewDrive", function() {
        saveAndMountDrive();
    });

    // 文本框右侧清除按钮 (Clear 'X' button)
    document.on("click", ".btn-clear-input", function(evt, clearBtn) {
        var shell = clearBtn.closest(".input-with-clear, .metro-input-shell");
        var input = shell ? shell.querySelector("input") : null;
        if (input) {
            input.value = "";
            input.dispatchEvent(new Event("change", { bubbles: true }));
            input.dispatchEvent(new Event("input", { bubbles: true }));
            if (typeof checkInputValidation === "function") {
                checkInputValidation();
            }
            input.focus();
        }
    });

    // 浏览私钥文件
    document.on("click", "#btnBrowsePrivateKey", function() {
        if (typeof Native_BrowsePrivateKey === "function") {
            var path = Native_BrowsePrivateKey();
            if (path) {
                var inputKey = document.getElementById("inputPrivateKey");
                if (inputKey) inputKey.value = path;
            }
        }
    });

    // 浏览缓存目录
    document.on("click", "#btnBrowseCachePath", function() {
        if (typeof Native_BrowseFolder === "function") {
            var path = Native_BrowseFolder();
            if (path) {
                var inputCache = document.getElementById("inputCachePath");
                if (inputCache) {
                    inputCache.value = path;
                    saveSettingsToNative();
                }
            }
        }
    });

    // 安装系统服务
    document.on("click", "#btnInstallService", function() {
        if (typeof Native_InstallService === "function") {
            try {
                var res = Native_InstallService();
                if (res) updateServiceStatus(res);
            } catch(e) {}
        }
    });

    // 卸载系统服务
    document.on("click", "#btnUninstallService", function() {
        if (typeof Native_UninstallService === "function") {
            try {
                var res = Native_UninstallService();
                if (res) updateServiceStatus(res);
            } catch(e) {}
        }
    });

    // 检查更新
    document.on("click", "#btnCheckUpdate", function() {
        if (typeof Native_GetRcloneVersion === "function") {
            var ver = Native_GetRcloneVersion();
            var lblVer = document.getElementById("lblUpdateStatus");
            if (lblVer && ver) lblVer.innerText = "已是最新版本 (" + ver + ")";
        }
    });

    // 单选框切换与反选逻辑
    document.on("click", "#lblAutoStartGUI", function() {
        var rGUI = document.getElementById("radioAutoStartGUI");
        var rNoGUI = document.getElementById("radioAutoStartNoGUI");
        if (rGUI.checked) {
            rGUI.checked = false;
        } else {
            rGUI.checked = true;
            rNoGUI.checked = false;
        }
        saveSettingsToNative();
    });

    document.on("click", "#lblAutoStartNoGUI", function() {
        var rGUI = document.getElementById("radioAutoStartGUI");
        var rNoGUI = document.getElementById("radioAutoStartNoGUI");
        if (rNoGUI.checked) {
            rNoGUI.checked = false;
        } else {
            rNoGUI.checked = true;
            rGUI.checked = false;
        }
        saveSettingsToNative();
    });

    document.on("change", "#chkOpenExplorer, #chkCustomIcon, #inputCachePath", function() {
        saveSettingsToNative();
    });

    document.on("click", "#btnDefaultCachePath", function() {
        var cacheIn = document.getElementById("inputCachePath");
        if (cacheIn) {
            cacheIn.value = "C:\\ProgramData\\rcloneGUI\\Cache";
            saveSettingsToNative();
        }
    });

    document.on("change", "#selLanguage", function(evt, el) {
        setLanguage(el.value);
        saveSettingsToNative();
    });

    // Sciter Event Delegation for Protocol Cards (WebDAV / SFTP / FTP)
    document.on("click", ".protocol-card", function(evt, card) {
        var proto = card.getAttribute("data-protocol");
        if (proto) {
            selectProtocol(proto);
        }
    });

    // Sciter Event Delegation for SSL Checkbox
    document.on("change", "#chkSSL", function(evt, el) {
        onSSLChange(el);
    });

    document.on("click", "#chkSSL", function(evt, el) {
        setTimeout(function() { onSSLChange(el); }, 0);
    });

    // Real-time input listener for red border validation
    document.on("change", "#inputHost, #inputUsername, #inputPassword", function() {
        checkInputValidation();
    });

    checkInputValidation();
    initTextContextMenus();

    // 绑定 Metro 原生 Custom Select 下拉框
    setupCustomSelect(
        document.getElementById("driveLetterSelect"),
        document.getElementById("driveLetterPopup"),
        function(val) {
            selectedDriveLetter = val;
        }
    );

    setupCustomSelect(
        document.getElementById("charsetSelect"),
        document.getElementById("charsetPopup"),
        function(val) {
            // charset changed
        }
    );

// ==========================================================================
// 10. Custom Metro Dropdown Popup Component (with Touch/Gesture Support)
// ==========================================================================
function setupCustomSelect(selectEl, popupEl, onChange) {
    if (!selectEl || !popupEl) return;
    var captionEl = selectEl.querySelector(".caption");
    var dismissedTime = 0;
    var isTouchDragging = false;
    var startTouchY = 0;
    var startScrollTop = 0;
    var hasScrolled = false;

    selectEl.addEventListener("mousedown", function (evt) {
        evt.preventDefault();
        if (Date.now() - dismissedTime < 150) return;
        popupEl.style.set({ width: selectEl.offsetWidth + "px" });
        selectEl.classList.add("open");
        selectEl.popup(popupEl, "bottom-left");
    });

    selectEl.addEventListener("popupdismissed", function () {
        dismissedTime = Date.now();
        selectEl.classList.remove("open");
    });

    popupEl.addEventListener("popupdismissed", function () {
        dismissedTime = Date.now();
        selectEl.classList.remove("open");
        isTouchDragging = false;
        hasScrolled = false;
        popupEl.querySelectorAll(".option-item.hot").forEach(function (item) {
            item.classList.remove("hot");
        });
    });

    // 1. Sciter 原生触摸手势生命周期（Gesture API）
    popupEl.addEventListener("gesture-start", function (evt) {
        if (typeof this.state?.wantsGestures === "function") {
            this.state.wantsGestures("pan-vertical");
        }
    });

    popupEl.addEventListener("gesture-pan", function (evt) {
        if (evt.deltaY) {
            hasScrolled = true;
            if (typeof this.scrollBy === "function") {
                this.scrollBy({ top: -evt.deltaY });
            } else {
                this.scrollTop -= evt.deltaY;
            }
            return true;
        }
    });

    // 2. 触摸/指针拖拽滚动支持（适配触摸屏模拟鼠标滑动）
    popupEl.addEventListener("mousedown", function (evt) {
        isTouchDragging = true;
        startTouchY = evt.y || evt.windowY || evt.clientY || 0;
        startScrollTop = popupEl.scrollTop || 0;
        hasScrolled = false;
    });

    popupEl.addEventListener("mousemove", function (evt) {
        if (isTouchDragging && (evt.buttons & 1 || evt.button === 0)) {
            var curY = evt.y || evt.windowY || evt.clientY || 0;
            var diff = curY - startTouchY;
            if (Math.abs(diff) > 4) {
                hasScrolled = true;
                popupEl.scrollTop = startScrollTop - diff;
                popupEl.querySelectorAll(".option-item.hot").forEach(function (oldItem) {
                    oldItem.classList.remove("hot");
                });
                return;
            }
        }

        if (!hasScrolled) {
            var item = evt.target.closest(".option-item");
            if (!item || item.classList.contains("hot")) return;
            popupEl.querySelectorAll(".option-item.hot").forEach(function (oldItem) {
                oldItem.classList.remove("hot");
            });
            item.classList.add("hot");
        }
    });

    popupEl.addEventListener("mouseup", function (evt) {
        isTouchDragging = false;
    });

    popupEl.addEventListener("click", function (evt) {
        if (hasScrolled) {
            // 滑动手势结束，阻止误选被滑过的项目
            hasScrolled = false;
            evt.preventDefault();
            evt.stopPropagation();
            return;
        }
        var item = evt.target.closest(".option-item");
        if (!item) return;
        var value = item.getAttribute("data-value");
        if (captionEl && value) captionEl.innerText = value;
        popupEl.state.popup = false;
        if (typeof onChange === "function") onChange(value);
    });

    popupEl.addEventListener("mouseleave", function () {
        isTouchDragging = false;
        popupEl.querySelectorAll(".option-item.hot").forEach(function (item) {
            item.classList.remove("hot");
        });
    });
}

// 全局通用触控手势委托：让所有 popup 下拉框、列表等容器均支持平滑触摸手势滑动
document.on("^gesture-start", "popup, select > popup, .metro-select-popup, .metro-popup, .drives-list, .metro-flyout-body", function (evt, el) {
    if (typeof el.state?.wantsGestures === "function") {
        el.state.wantsGestures("pan-vertical");
    }
});

document.on("^gesture-pan", "popup, select > popup, .metro-select-popup, .metro-popup, .drives-list, .metro-flyout-body", function (evt, el) {
    if (evt.deltaY) {
        if (typeof el.scrollBy === "function") {
            el.scrollBy({ top: -evt.deltaY });
        } else if (typeof el.scrollTop === "number") {
            el.scrollTop -= evt.deltaY;
        }
        return true;
    }
});

// ==========================================================================
// 11. Text Editor Context Menu System
// ==========================================================================
var activeTextContextTarget = null;
var activeTextSelectionSnapshot = null;
var executingTextMenuCommand = false;
var textSelectionSnapshots = new WeakMap();
var textPlaceholderSnapshots = new WeakMap();
var textEditCommands = {
    "undo": "edit:undo",
    "cut": "edit:cut",
    "copy": "edit:copy",
    "paste": "edit:paste",
    "delete": "edit:delete-next",
    "select-all": "edit:selectall"
};

function getCursorPosition(evt) {
    var cursor = null;
    try {
        if (typeof Window !== "undefined" && Window.this && typeof Window.this.cursorPos === "function") {
            cursor = Window.this.cursorPos();
        }
    } catch (error) {
        cursor = null;
    }

    var x = cursor && cursor.length > 1 ? cursor[0] : evt.windowX;
    var y = cursor && cursor.length > 1 ? cursor[1] : evt.windowY;
    if (typeof x !== "number") x = evt.clientX || 0;
    if (typeof y !== "number") y = evt.clientY || 0;
    return { x: x, y: y };
}

function getTextEditor(target) {
    return target.edit || target.password || target.textarea;
}

function captureTextSelection(target) {
    var editor = getTextEditor(target);
    if (!editor) return { text: "", start: 0, end: 0 };
    var snapshot = {
        text: editor.selectionText || "",
        start: editor.selectionStart,
        end: editor.selectionEnd
    };
    textSelectionSnapshots.set(target, snapshot);
    return snapshot;
}

function restoreTextSelection(target, snapshot) {
    target.focus();
    var editor = getTextEditor(target);
    if (editor && typeof editor.selectRange === "function" && snapshot) {
        editor.selectRange(snapshot.start, snapshot.end);
    }
}

function clearTextSelection(target) {
    var snapshot = captureTextSelection(target);
    var editor = getTextEditor(target);
    if (editor && typeof editor.selectRange === "function") {
        editor.selectRange(snapshot.end, snapshot.end);
    } else if (typeof target.setSelectionRange === "function") {
        var valLen = target.value ? target.value.length : 0;
        target.setSelectionRange(valLen, valLen);
    }
}

function setTextMenuOpen(target, open) {
    if (open) {
        target.setAttribute("text-menu-open", "");
        if (target.hasAttribute("placeholder") && !textPlaceholderSnapshots.has(target)) {
            textPlaceholderSnapshots.set(target, target.getAttribute("placeholder"));
            target.removeAttribute("placeholder");
        }
    } else {
        target.removeAttribute("text-menu-open");
        if (textPlaceholderSnapshots.has(target)) {
            target.setAttribute("placeholder", textPlaceholderSnapshots.get(target));
            textPlaceholderSnapshots.delete(target);
        }
    }
}

function canRunTextCommand(target, command) {
    var nativeCommand = textEditCommands[command];
    if (!nativeCommand) return false;
    var readonly = target.hasAttribute("readonly") || target.hasAttribute("disabled");
    var hasSelection = activeTextSelectionSnapshot && activeTextSelectionSnapshot.text.length > 0;
    var isPassword = (target.getAttribute("type") || "").toLowerCase() === "password";

    if (command === "copy") return !isPassword && hasSelection;
    if (command === "select-all") return target.value.length > 0;
    if (command === "cut" || command === "delete") return !readonly && !isPassword && hasSelection;
    if (command === "undo") return !readonly && target.hasAttribute("data-can-undo");

    try {
        if (typeof target.checkCommand === "function") {
            return (target.checkCommand(nativeCommand) & 0x02) === 0;
        }
    } catch (error) {
    }

    if (command === "paste") return !readonly;
    return false;
}

function showTextContextMenu(evt, target) {
    var menu = document.getElementById("textContextMenu");
    if (!menu) return;
    target.focus();
    if (target.state) target.state.focus = true;
    activeTextContextTarget = target;
    setTextMenuOpen(target, true);
    var editor = getTextEditor(target);
    var currentSelection = (editor && editor.selectionText) || "";
    activeTextSelectionSnapshot = currentSelection.length > 0
        ? captureTextSelection(target)
        : (textSelectionSnapshots.get(target) || captureTextSelection(target));
    
    var items = menu.querySelectorAll(".context-command-item");
    for (var i = 0; i < items.length; i++) {
        var item = items[i];
        if (canRunTextCommand(target, item.getAttribute("data-command"))) {
            item.removeAttribute("disabled");
        } else {
            item.setAttribute("disabled", "");
        }
    }
    var cursor = getCursorPosition(evt);
    document.body.popup(menu, {
        anchorAt: 7,
        popupAt: 7,
        x: cursor.x,
        y: cursor.y
    });
}

function runTextMenuCommand(item) {
    if (item.hasAttribute("disabled") || !activeTextContextTarget) return;
    executingTextMenuCommand = true;
    var command = item.getAttribute("data-command");
    var nativeCommand = textEditCommands[command];
    var editorApi = getTextEditor(activeTextContextTarget);
    var exec = typeof activeTextContextTarget.execCommand === "function"
        ? activeTextContextTarget.execCommand.bind(activeTextContextTarget)
        : (activeTextContextTarget.executeCommand ? activeTextContextTarget.executeCommand.bind(activeTextContextTarget) : null);

    if (command !== "copy") restoreTextSelection(activeTextContextTarget, activeTextSelectionSnapshot);
    if (command === "cut" && exec) exec("edit:cut");
    else if (command === "delete" && activeTextContextTarget.textarea) activeTextContextTarget.textarea.removeText();
    else if (command === "copy" && exec) exec("edit:copy");
    else if (command === "select-all" && editorApi) editorApi.selectAll();
    else if (exec && nativeCommand) exec(nativeCommand);

    var menu = document.getElementById("textContextMenu");
    if (menu && menu.state) menu.state.popup = false;
    if (command === "copy" && activeTextContextTarget.state) activeTextContextTarget.state.focus = true;
    setTextMenuOpen(activeTextContextTarget, false);
    executingTextMenuCommand = false;
}

function initTextContextMenus() {
    var menu = document.getElementById("textContextMenu");
    if (!menu) return;

    menu.addEventListener("popupdismissed", function () {
        if (!activeTextContextTarget) return;
        setTextMenuOpen(activeTextContextTarget, false);
        if (!executingTextMenuCommand && activeTextContextTarget.state && !activeTextContextTarget.state.focus) {
            clearTextSelection(activeTextContextTarget);
        }
    });

    menu.addEventListener("mouseup", function (evt) {
        var item = evt.target.closest(".context-command-item");
        if (!item) return;
        evt.preventDefault();
        evt.stopPropagation();
        runTextMenuCommand(item);
    });

    document.on("focus", 'input[type="text"], input[type="password"], textarea, .metro-text-input', function (evt, target) {
        if (activeTextContextTarget && activeTextContextTarget !== target &&
            activeTextContextTarget.hasAttribute("text-menu-open")) {
            setTextMenuOpen(activeTextContextTarget, false);
            clearTextSelection(activeTextContextTarget);
        }
    });

    document.on("blur", 'input[type="text"], input[type="password"], textarea, .metro-text-input', function (evt, target) {
        if (!target.hasAttribute("text-menu-open")) {
            clearTextSelection(target);
        }
    });

    document.on("keyup", 'input[type="text"], input[type="password"], textarea, .metro-text-input', function (evt, target) {
        captureTextSelection(target);
    });

    document.on("input", 'input[type="text"], input[type="password"], textarea, .metro-text-input', function (evt, target) {
        if (!target.hasAttribute("readonly")) target.setAttribute("data-can-undo", "true");
        captureTextSelection(target);
    });

    document.on("mouseup", 'input[type="text"], input[type="password"], textarea, .metro-text-input', function (evt, target) {
        if (evt.button === 2) return;
        captureTextSelection(target);
    });

    document.on("mousedown", 'input[type="text"], input[type="password"], textarea, .metro-text-input', function (evt, target) {
        if (evt.button !== 2) return;
        evt.preventDefault();
        evt.stopPropagation();
        showTextContextMenu(evt, target);
    });

    document.on("contextmenu", 'input[type="text"], input[type="password"], textarea, .metro-text-input', function (evt) {
        evt.preventDefault();
        evt.stopPropagation();
    });
}

document.on("click", ".btn-toggle-drive", function (evt, target) {
    var el = target.closest("[data-index]") || target;
    var idx = parseInt(el.getAttribute("data-index"));
    if (!isNaN(idx)) toggleDriveStatus(idx);
});

document.on("click", ".btn-edit-drive", function (evt, target) {
    var el = target.closest("[data-index]") || target;
    var idx = parseInt(el.getAttribute("data-index"));
    if (!isNaN(idx)) editDrive(idx);
});

document.on("click", ".btn-open-drive", function (evt, target) {
    var el = target.closest("[data-index]") || target;
    var idx = parseInt(el.getAttribute("data-index"));
    if (!isNaN(idx)) openDriveLetter(idx);
});

document.on("click", ".btn-delete-drive", function (evt, target) {
    var el = target.closest("[data-index]") || target;
    var idx = parseInt(el.getAttribute("data-index"));
    if (!isNaN(idx)) deleteDrive(idx);
});

// Sciter's CSS :hover/:active is unreliable on these clickable divs.
// Track the pointer explicitly so only the border and inner background change.
var hoveredDriveCircle = null;
function updateDriveCircleVisual(button, state) {
    if (!button || button.classList.contains("is-connecting")) return;
    var bg = button.querySelector("circle.circle-bg");
    var stop = button.querySelector("circle.circle-stop");
    var fill = state === "active" ? "#777777" : (state === "hover" ? "#484848" : "#333333");
    var edge = state === "active" ? "#e0e0e0" : (state === "hover" ? "#e0e0e0" : "#777777");
    if (bg) {
        bg.setAttribute("fill", fill);
        bg.setAttribute("stroke", edge);
    }
    if (stop) {
        var stopColor = state === "active" ? "#777777" : (state === "hover" ? "#484848" : "#333333");
        var stopEdge = state === "active" ? "#e0e0e0" : (state === "hover" ? "#e0e0e0" : "#777777");
        stop.setAttribute("fill", stopColor);
        stop.setAttribute("stroke", stopEdge);
        var stopSquare = button.querySelector("rect");
        if (stopSquare) stopSquare.setAttribute("fill", "#ffffff");
    }
}

document.on("mousemove", function (evt, target) {
    var next = target && target.closest ? target.closest(".btn-drive-circle") : null;
    if (hoveredDriveCircle && hoveredDriveCircle !== next) {
        hoveredDriveCircle.classList.remove("is-hover");
        updateDriveCircleVisual(hoveredDriveCircle, "normal");
    }
    if (next && next !== hoveredDriveCircle) {
        next.classList.add("is-hover");
        updateDriveCircleVisual(next, "hover");
    }
    hoveredDriveCircle = next;
});

document.on("mousedown", ".btn-drive-circle", function (evt, target) {
    target.classList.add("is-active");
    updateDriveCircleVisual(target, "active");
});

document.on("mouseup", ".btn-drive-circle", function (evt, target) {
    target.classList.remove("is-active");
    updateDriveCircleVisual(target, "hover");
});



globalThis.openNewDrivePanel = openNewDrivePanel;
globalThis.editDrive = editDrive;

// Windows 10 ProgressRing (追逐跟点/变速聚散特效)
function getWin10Angle(p) {
    if (p < 0.25) {
        var u = p / 0.25;
        return (u * u * (3 - 2 * u)) * 180;
    } else if (p < 0.65) {
        var u = (p - 0.25) / 0.40;
        return 180 + Math.sin(u * Math.PI / 2) * 270;
    } else if (p < 0.95) {
        var u = (p - 0.65) / 0.30;
        return 450 + (u * u) * 270;
    } else {
        return 720;
    }
}

function getWin10Opacity(p) {
    if (p < 0.06) return p / 0.06;
    if (p > 0.88) return Math.max(0, (0.96 - p) / 0.08);
    return 1.0;
}

var ringStartTime = Date.now();
setInterval(function () {
    var rings = document.querySelectorAll(".win10-progress-ring");
    if (!rings || rings.length === 0) return;

    var now = (Date.now() - ringStartTime) / 1000;
    var cycleDuration = 4.2;

    for (var r = 0; r < rings.length; r++) {
        var dots = rings[r].querySelectorAll(".wdot");
        for (var i = 0; i < dots.length; i++) {
            var delay = i * 0.14;
            var dt = now - delay;
            if (dt < 0) {
                dots[i].setAttribute("opacity", "0");
                dots[i].style.opacity = 0;
                continue;
            }
            var p = (dt % cycleDuration) / cycleDuration;
            var angle = getWin10Angle(p);
            var rad = (angle + 25) * Math.PI / 180;
            var cx = 17 + 14.5 * Math.cos(rad);
            var cy = 17 + 14.5 * Math.sin(rad);
            var op = getWin10Opacity(p);

            var cxStr = cx.toFixed(2);
            var cyStr = cy.toFixed(2);
            var opStr = op.toFixed(2);

            dots[i].setAttribute("cx", cxStr);
            dots[i].setAttribute("cy", cyStr);
            dots[i].setAttribute("opacity", opStr);
            dots[i].style.opacity = op;
        }
        if (typeof rings[r].requestPaint === "function") {
            try { rings[r].requestPaint(); } catch (e) {}
        }
        var canvas = rings[r].closest(".win10-spinner-canvas");
        if (canvas && typeof canvas.requestPaint === "function") {
            try { canvas.requestPaint(); } catch (e) {}
        }
    }
}, 16);

// Base64 helper for authorization
function base64Encode(str) {
    const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
    let output = '';
    let i = 0;
    while (i < str.length) {
        const c1 = str.charCodeAt(i++);
        const c2 = i < str.length ? str.charCodeAt(i++) : NaN;
        const c3 = i < str.length ? str.charCodeAt(i++) : NaN;
        
        const byte1 = c1 >> 2;
        const byte2 = ((c1 & 3) << 4) | (isNaN(c2) ? 0 : (c2 >> 4));
        const byte3 = isNaN(c2) ? 64 : (((c2 & 15) << 2) | (isNaN(c3) ? 0 : (c3 >> 6)));
        const byte4 = isNaN(c3) ? 64 : (c3 & 63);
        
        output += chars.charAt(byte1) + chars.charAt(byte2) +
                  (byte3 === 64 ? '=' : chars.charAt(byte3)) +
                  (byte4 === 64 ? '=' : chars.charAt(byte4));
    }
    return output;
}

// Get RC settings from native
function getRCSettings() {
    let settings = { rcPort: 5572, rcUser: "admin", rcPass: "admin123" };
    if (typeof Native_GetSettings === "function") {
        try {
            let raw = Native_GetSettings();
            let s = typeof raw === "string" ? JSON.parse(raw) : raw;
            if (s) {
                if (s.rcPort) settings.rcPort = s.rcPort;
                if (s.rcUser) settings.rcUser = s.rcUser;
                if (s.rcPass) settings.rcPass = s.rcPass;
            }
        } catch (e) {}
    }
    return settings;
}

var pendingRCRequests = {};
var rcloneRCSeq = 0;

function onNativeRcloneRCResponse(id, respStr) {
    var handler = pendingRCRequests[id];
    if (handler) {
        delete pendingRCRequests[id];
        try {
            var data = typeof respStr === "string" ? JSON.parse(respStr) : respStr;
            handler.resolve(data);
        } catch (e) {
            handler.resolve({ error: "JSON parse error: " + e });
        }
    }
}
globalThis.onNativeRcloneRCResponse = onNativeRcloneRCResponse;

// Call Rclone RC API asynchronously via background C++ worker thread
function callRcloneRC(method, params) {
    if (typeof Native_CallRcloneRCAsync === "function") {
        return new Promise((resolve, reject) => {
            var reqId = ++rcloneRCSeq;
            pendingRCRequests[reqId] = { resolve, reject };
            try {
                Native_CallRcloneRCAsync({
                    id: reqId,
                    method: method,
                    body: JSON.stringify(params || {})
                });
            } catch (err) {
                delete pendingRCRequests[reqId];
                resolve({ error: err.toString() });
            }
        });
    } else if (typeof Native_CallRcloneRC === "function") {
        try {
            let req = {
                method: method,
                body: JSON.stringify(params || {})
            };
            let raw = Native_CallRcloneRC(req);
            let data = typeof raw === "string" ? JSON.parse(raw) : raw;
            return Promise.resolve(data);
        } catch (e) {
            return Promise.resolve({ error: e.toString() });
        }
    }
    return Promise.resolve({ error: "Native_CallRcloneRC not available" });
}

// Sync drive configuration to running Rclone daemon memory
async function syncDriveConfigToRC(drive, isPlaintext) {
    let proto = (drive.protocol || "ftp").toLowerCase();
    let parameters = {};
    if (proto === "ftp") {
        parameters.host = drive.host || "";
        parameters.port = drive.port ? String(drive.port) : "21";
        parameters.user = drive.username || "";
        parameters.pass = drive.password || "";
        parameters.tls = drive.isSSL ? "true" : "false";
        parameters.explicit_tls = drive.isExplicit ? "true" : "false";
        parameters.pass_mode = drive.isPassive ? "passive" : "active";
        parameters.disable_mlsd = "true";
        parameters.idle_timeout = "10s";
    } else if (proto === "webdav") {
        let scheme = drive.isSSL ? "https://" : "http://";
        let url = scheme + (drive.host || "");
        if (drive.port) url += ":" + drive.port;
        if (drive.path && drive.path !== "undefined") {
            let p = drive.path.trim();
            if (p) {
                if (!p.startsWith("/")) p = "/" + p;
                url += p;
            }
        }
        parameters.url = url;
        parameters.vendor = "other";
        parameters.user = drive.username || "";
        parameters.pass = drive.password || "";
    } else if (proto === "sftp") {
        parameters.host = drive.host || "";
        parameters.port = drive.port ? String(drive.port) : "22";
        parameters.user = drive.username || "";
        parameters.pass = drive.password || "";
        if (drive.privateKey) parameters.key_file = drive.privateKey;
    }
    
    let configParams = {
        name: drive.name,
        type: proto,
        parameters: parameters
    };
    if (isPlaintext) {
        configParams.opt = { obscure: true };
    }
    let res = await callRcloneRC("config/create", configParams);
    
    // If it was newly encrypted, query the new obscured password back from daemon and save
    if (isPlaintext && drive.name) {
        try {
            let getRes = await callRcloneRC("config/get", { name: drive.name });
            if (getRes && getRes.pass) {
                drive.password = getRes.pass;
                if (typeof Native_SaveDrive === "function") {
                    try { Native_SaveDrive(drive); } catch (e) {}
                }
            }
        } catch (e) {}
    }
    return res;
}

// HTTP Mount Function
async function mountDriveHTTP(drive) {
    if (typeof Native_SaveDrive === "function") {
        try { Native_SaveDrive(drive); } catch (e) {}
    }
    
    let isLocal = !!drive.isLocalDisk;
    let isReadOnly = !!drive.isReadOnly;
    
    let mountOpt = {
        VolumeName: drive.name || "",
        NetworkMode: !isLocal, // 未勾选“本地磁盘”时，作为网络驱动器(Network Drive)挂载至网络位置
        AttrTimeout: 1000000000
    };
    
    let vfsOpt = {
        CacheMode: 2, // writes
        Links: true,
        ReadOnly: isReadOnly,
        DirCacheTime: 3000000000, // 3s
        WriteBack: 0,
        ChunkSize: 33554432, // 32MB (分块读取起始大小)
        ChunkSizeLimit: 536870912, // 512MB (分块上限自动倍增)
        ReadAhead: 67108864 // 64MB (预读缓冲大小)
    };
    
    let cachePath = "C:\\ProgramData\\rcloneGUI\\Cache";
    if (typeof Native_GetSettings === "function") {
        try {
            let raw = Native_GetSettings();
            let s = typeof raw === "string" ? JSON.parse(raw) : raw;
            if (s && s.cachePath) cachePath = s.cachePath;
        } catch (e) {}
    }
    // CacheDir 是 rclone 全局 main 选项，不属于 vfsOpt。
    // 在挂载前同步全局缓存根目录，VFS 会自行在其下创建 vfs/vfsMeta 层级。
    try {
        await callRcloneRC("options/set", { main: { CacheDir: cachePath } });
    } catch (e) {}
    
    let mountPoint = drive.letter;
    if (!mountPoint || mountPoint === "Auto" || mountPoint === "*") {
        mountPoint = getFirstAvailableDriveLetter();
    }
    let fsParam = drive.name + ":";
    let proto = (drive.protocol || "").toLowerCase();
    if (proto === "ftp" || proto === "sftp") {
        if (drive.path && drive.path !== "undefined") {
            let p = drive.path.trim();
            if (p && p !== "/") {
                if (!p.startsWith("/")) p = "/" + p;
                fsParam = drive.name + ":" + p;
            }
        }
    }
    let params = {
        fs: fsParam,
        mountPoint: mountPoint,
        mountOpt: mountOpt,
        vfsOpt: vfsOpt
    };
    
    let res = await callRcloneRC("mount/mount", params);
    if (res && res.error) {
        // If config section was not found in running daemon, sync it once and retry mount
        if (res.error.indexOf("didn't find section") !== -1 || res.error.indexOf("not found") !== -1) {
            logDebug("[mountDriveHTTP] Section not found in daemon, creating config...");
            await syncDriveConfigToRC(drive);
            res = await callRcloneRC("mount/mount", params);
        }
    }
    
    if (res && res.error) {
        return { success: false, error: res.error };
    }

    drive.currentMountedLetter = mountPoint;
    return { success: true };
}

// HTTP Unmount Function
async function unmountDriveHTTP(driveName) {
    let drive = mountedDrives.find(d => d.name === driveName);
    if (!drive) return { success: false, error: "未找到指定的驱动器" };
    
    let mountPoint = drive.currentMountedLetter || drive.letter;
    if (!mountPoint || mountPoint === "Auto" || mountPoint === "*") {
        if (typeof Native_GetMountedDriveLetter === "function") {
            try { mountPoint = Native_GetMountedDriveLetter(drive.name); } catch(e) {}
        }
    }
    if (!mountPoint || mountPoint === "Auto" || mountPoint === "*") {
        try {
            let listRes = await callRcloneRC("mount/listmounts", {});
            let mountPoints = listRes.mountPoints || [];
            let match = mountPoints.find(mp => {
                let fsStr = typeof mp === "object" ? (mp.Fs || mp.fs || "") : (typeof mp === "string" ? mp : "");
                let rName = fsStr.split(":")[0].trim().toUpperCase();
                return rName === drive.name.toUpperCase();
            });
            if (match) {
                mountPoint = typeof match === "string" ? match : (match.MountPoint || match.mountPoint || "");
            }
        } catch (e) {}
    }
    
    let params = {
        mountPoint: mountPoint
    };
    
    let res = await callRcloneRC("mount/unmount", params);
    if (res && res.error) {
        return { success: false, error: res.error };
    }
    return { success: true };
}





