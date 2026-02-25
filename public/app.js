// API Base URL
const API_BASE = 'http://localhost:8080';

// State
let currentUser = 'Misafir';
let currentRoomId = 'global';
let refreshInterval;
let realtimeSocket = null;
let realtimeConnected = false;
let reconnectTimer = null;

function getApiToken() {
    return localStorage.getItem('apiToken') || '';
}

function buildApiHeaders(extraHeaders = {}) {
    const headers = { ...extraHeaders };
    const token = getApiToken();
    if (token) {
        if (!headers.Authorization) {
            headers.Authorization = `Bearer ${token}`;
        }
        if (!headers['X-API-Token']) {
            headers['X-API-Token'] = token;
        }
    }
    return headers;
}

function apiFetch(path, options = {}) {
    return fetch(`${API_BASE}${path}`, {
        ...options,
        headers: buildApiHeaders(options.headers || {})
    });
}

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    initializeApp();
});

function initializeApp() {
    // Setup event listeners
    setupTabNavigation();

    // Load username
    const usernameInput = document.getElementById('username');
    usernameInput.value = localStorage.getItem('username') || 'Misafir';
    currentUser = usernameInput.value;

    usernameInput.addEventListener('change', (e) => {
        currentUser = e.target.value;
        localStorage.setItem('username', currentUser);
        showNotification('Kullanıcı adı güncellendi', 'success');
        if (realtimeSocket && realtimeSocket.readyState === WebSocket.OPEN) {
            realtimeSocket.close();
        } else {
            connectRealtime();
        }
    });

    const roomInput = document.getElementById('chatRoomId');
    if (roomInput) {
        roomInput.value = localStorage.getItem('chatRoomId') || 'global';
        currentRoomId = (roomInput.value || 'global').trim() || 'global';
        updateRoomLabel();

        roomInput.addEventListener('change', () => {
            currentRoomId = (roomInput.value || 'global').trim() || 'global';
            roomInput.value = currentRoomId;
            localStorage.setItem('chatRoomId', currentRoomId);
            updateRoomLabel();
            loadChatMessages();
            if (realtimeSocket && realtimeSocket.readyState === WebSocket.OPEN) {
                realtimeSocket.close();
            } else {
                connectRealtime();
            }
        });
    }

    const chatInput = document.getElementById('chatMessageInput');
    if (chatInput) {
        chatInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                sendChatMessage();
            }
        });
    }

    // Check server health
    checkServerHealth();

    // Load initial data
    // Load initial data
    initNotepad();
    loadChatMessages();
    loadActiveVoiceCalls();
    loadActiveVideoCalls();
    loadLiveStreams();
    loadRecordings();
    refreshStats();

    // Realtime websocket
    connectRealtime();

    // Auto-refresh
    startAutoRefresh();
}

// Tab Navigation
function setupTabNavigation() {
    const navItems = document.querySelectorAll('.nav-item');
    navItems.forEach(item => {
        item.addEventListener('click', (e) => {
            e.preventDefault();
            const tab = item.dataset.tab;
            switchTab(tab);
        });
    });
}

function switchTab(tabName) {
    // Update nav
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.remove('active');
    });
    document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');

    // Update content
    document.querySelectorAll('.tab-content').forEach(content => {
        content.classList.remove('active');
    });
    document.getElementById(`${tabName}-tab`).classList.add('active');

    // Refresh tab data
    if (tabName === 'stats') {
        refreshStats();
    } else if (tabName === 'notepad') {
        loadChatMessages();
    }
}

// function setupEnterKeyListeners removed

// Server Health Check
async function checkServerHealth() {
    try {
        const response = await apiFetch(`/api/health`);
        const data = await response.json();

        const statusEl = document.getElementById('serverStatus');
        statusEl.classList.add('online');
        statusEl.querySelector('.status-text').textContent = 'Çevrimiçi';

        document.getElementById('lastUpdate').textContent = data.timestamp;
    } catch (error) {
        const statusEl = document.getElementById('serverStatus');
        statusEl.classList.remove('online');
        statusEl.classList.add('offline');
        statusEl.querySelector('.status-text').textContent = 'Çevrimdışı';
        showNotification('Sunucuya bağlanılamadı', 'error');
    }
}

function getWebSocketUrl() {
    const wsBase = API_BASE.replace(/^http:/i, 'ws:').replace(/^https:/i, 'wss:');
    const room = encodeURIComponent(currentRoomId || 'global');
    const user = encodeURIComponent(currentUser || 'Misafir');
    const token = localStorage.getItem('wsToken') || getApiToken();
    const tokenPart = token ? `&token=${encodeURIComponent(token)}` : '';
    return `${wsBase}/ws?room=${room}&user=${user}${tokenPart}`;
}

function connectRealtime() {
    if (realtimeSocket && (realtimeSocket.readyState === WebSocket.OPEN || realtimeSocket.readyState === WebSocket.CONNECTING)) {
        return;
    }

    try {
        realtimeSocket = new WebSocket(getWebSocketUrl());
    } catch (error) {
        scheduleRealtimeReconnect();
        return;
    }

    realtimeSocket.onopen = () => {
        realtimeConnected = true;
        if (reconnectTimer) {
            clearTimeout(reconnectTimer);
            reconnectTimer = null;
        }
    };

    realtimeSocket.onmessage = (event) => {
        handleRealtimeEvent(event.data);
    };

    realtimeSocket.onerror = () => {
        realtimeConnected = false;
    };

    realtimeSocket.onclose = () => {
        realtimeConnected = false;
        scheduleRealtimeReconnect();
    };
}

function scheduleRealtimeReconnect() {
    if (reconnectTimer) return;
    reconnectTimer = setTimeout(() => {
        reconnectTimer = null;
        connectRealtime();
    }, 2000);
}

function handleRealtimeEvent(raw) {
    let evt;
    try {
        evt = JSON.parse(raw);
    } catch (error) {
        return;
    }

    if (!evt || !evt.type) return;

    if (evt.type.startsWith('voice.call.')) {
        loadActiveVoiceCalls();
        refreshStats();
        return;
    }

    if (evt.type.startsWith('video.call.')) {
        loadActiveVideoCalls();
        refreshStats();
        return;
    }

    if (evt.type.startsWith('stream.')) {
        loadLiveStreams();
        refreshStats();
        return;
    }

    if (evt.type.startsWith('recording.')) {
        loadRecordings();
        refreshStats();
        return;
    }

    if (evt.type.startsWith('chat.')) {
        if (!evt.roomId || evt.roomId === currentRoomId) {
            loadChatMessages();
        }
        refreshStats();
    }
}

// ============================================================================
// CHAT FUNCTIONS
// ============================================================================

function updateRoomLabel() {
    const label = document.getElementById('chatRoomLabel');
    if (label) {
        label.textContent = currentRoomId || 'global';
    }
}

async function loadChatMessages() {
    try {
        const roomParam = encodeURIComponent(currentRoomId || 'global');
        const response = await apiFetch(`/api/chat/messages?roomId=${roomParam}`);
        const messages = await response.json();
        renderChatMessages(messages);
    } catch (error) {
        console.error('Failed to load chat messages:', error);
    }
}

function renderChatMessages(messages) {
    const listEl = document.getElementById('chatMessages');
    if (!listEl) return;

    if (!Array.isArray(messages) || messages.length === 0) {
        listEl.innerHTML = `
            <div class="empty-state">
                <i class="fas fa-comment-slash"></i>
                <p>Bu odada henuz mesaj yok</p>
            </div>
        `;
        return;
    }

    listEl.innerHTML = messages.map((msg) => `
        <div style="padding: 8px 0; border-bottom: 1px solid rgba(148,163,184,0.18);">
            <div style="display:flex; justify-content:space-between; gap:8px;">
                <strong style="color:#93c5fd;">${escapeHtml(msg.username || 'Misafir')}</strong>
                <span style="color:#94a3b8; font-size:12px;">${escapeHtml(msg.timestamp || '')}</span>
            </div>
            <div style="margin-top:4px; color:#e2e8f0; white-space:pre-wrap;">${escapeHtml(msg.content || '')}</div>
        </div>
    `).join('');

    listEl.scrollTop = listEl.scrollHeight;
}

async function sendChatMessage() {
    const input = document.getElementById('chatMessageInput');
    const content = (input?.value || '').trim();
    if (!content) {
        return;
    }

    try {
        const response = await apiFetch(`/api/chat/messages`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                username: currentUser || 'Misafir',
                content: content,
                roomId: currentRoomId || 'global'
            })
        });

        if (!response.ok) {
            throw new Error('Mesaj gonderilemedi');
        }

        input.value = '';

        // Fallback for non-realtime mode
        if (!realtimeConnected) {
            loadChatMessages();
        }
        refreshStats();
    } catch (error) {
        showNotification(error.message || 'Mesaj gonderilemedi', 'error');
    }
}

// ============================================================================
// NOTEPAD FUNCTIONS
// ============================================================================

function initNotepad() {
    const notepad = document.getElementById('notepadArea');
    const saveStatus = document.getElementById('saveStatus');

    // Load saved note
    const savedNote = localStorage.getItem('userNote');
    if (savedNote) {
        notepad.value = savedNote;
    }

    let debounceTimer;
    notepad.addEventListener('input', () => {
        clearTimeout(debounceTimer);
        saveStatus.style.display = 'none';

        debounceTimer = setTimeout(() => {
            localStorage.setItem('userNote', notepad.value);
            saveStatus.style.display = 'inline';
            setTimeout(() => {
                saveStatus.style.display = 'none';
            }, 2000);
        }, 1000); // Save after 1 second of inactivity
    });
}

// ============================================================================
// VOICE CALL FUNCTIONS
// ============================================================================

async function startVoiceCall() {
    const receiverId = document.getElementById('voiceReceiverId').value.trim();
    const receiverName = document.getElementById('voiceReceiverName').value.trim();

    if (!receiverId || !receiverName) {
        showNotification('Lütfen tüm alanları doldurun', 'error');
        return;
    }

    try {
        const response = await apiFetch(`/api/voice/call`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                callerId: `user_${currentUser}`,
                callerName: currentUser,
                receiverId: receiverId,
                receiverName: receiverName
            })
        });

        if (response.ok) {
            const data = await response.json();
            showNotification(`${receiverName} aranıyor...`, 'success');
            document.getElementById('voiceReceiverId').value = '';
            document.getElementById('voiceReceiverName').value = '';
            loadActiveVoiceCalls();
        }
    } catch (error) {
        showNotification('Arama başlatılamadı', 'error');
    }
}

async function loadActiveVoiceCalls() {
    try {
        const response = await apiFetch(`/api/voice/active`);
        const calls = await response.json();

        const listEl = document.getElementById('activeVoiceCalls');

        if (calls.length === 0) {
            listEl.innerHTML = `
                <div class="empty-state">
                    <i class="fas fa-phone-slash"></i>
                    <p>Aktif arama yok</p>
                </div>
            `;
        } else {
            listEl.innerHTML = calls.map(call => `
                <div class="call-item">
                    <div class="call-info">
                        <h4>${escapeHtml(call.callerName)} → ${escapeHtml(call.receiverName)}</h4>
                        <p>${call.startTime}</p>
                    </div>
                    <div class="call-actions">
                        <span class="call-status ${call.status}">${getStatusText(call.status)}</span>
                        ${call.status !== 'ended' ? `
                            <button class="btn btn-sm btn-danger" onclick="endVoiceCall('${call.id}')">
                                <i class="fas fa-phone-slash"></i>
                            </button>
                        ` : ''}
                    </div>
                </div>
            `).join('');
        }

        updateBadge('voiceBadge', calls.filter(c => c.status !== 'ended').length);
    } catch (error) {
        console.error('Failed to load voice calls:', error);
    }
}

async function endVoiceCall(callId) {
    try {
        await apiFetch(`/api/voice/end/${callId}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: '{}'
        });
        showNotification('Arama sonlandırıldı', 'info');
        loadActiveVoiceCalls();
    } catch (error) {
        showNotification('Arama sonlandırılamadı', 'error');
    }
}

// ============================================================================
// VIDEO CALL FUNCTIONS
// ============================================================================

async function startVideoCall() {
    const receiverId = document.getElementById('videoReceiverId').value.trim();
    const receiverName = document.getElementById('videoReceiverName').value.trim();

    if (!receiverId || !receiverName) {
        showNotification('Lütfen tüm alanları doldurun', 'error');
        return;
    }

    try {
        const response = await apiFetch(`/api/video/call`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                callerId: `user_${currentUser}`,
                callerName: currentUser,
                receiverId: receiverId,
                receiverName: receiverName,
                offer: 'WEBRTC_SDP_OFFER_DATA'
            })
        });

        if (response.ok) {
            const data = await response.json();
            showNotification(`Video araması başlatıldı: ${receiverName}`, 'success');
            document.getElementById('videoReceiverId').value = '';
            document.getElementById('videoReceiverName').value = '';
            loadActiveVideoCalls();
        }
    } catch (error) {
        showNotification('Video araması başlatılamadı', 'error');
    }
}

async function loadActiveVideoCalls() {
    try {
        const response = await apiFetch(`/api/video/active`);
        const calls = await response.json();

        const listEl = document.getElementById('activeVideoCalls');

        if (calls.length === 0) {
            listEl.innerHTML = `
                <div class="empty-state">
                    <i class="fas fa-video-slash"></i>
                    <p>Aktif video araması yok</p>
                </div>
            `;
        } else {
            listEl.innerHTML = calls.map(call => `
                <div class="call-item">
                    <div class="call-info">
                        <h4><i class="fas fa-video"></i> ${escapeHtml(call.callerName)} → ${escapeHtml(call.receiverName)}</h4>
                        <p>${call.startTime}</p>
                    </div>
                    <div class="call-actions">
                        <span class="call-status ${call.status}">${getStatusText(call.status)}</span>
                        ${call.status !== 'ended' ? `
                            <button class="btn btn-sm btn-danger" onclick="endVideoCall('${call.id}')">
                                <i class="fas fa-phone-slash"></i>
                            </button>
                        ` : ''}
                    </div>
                </div>
            `).join('');
        }

        updateBadge('videoBadge', calls.filter(c => c.status !== 'ended').length);
    } catch (error) {
        console.error('Failed to load video calls:', error);
    }
}

async function endVideoCall(callId) {
    try {
        await apiFetch(`/api/video/end/${callId}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: '{}'
        });
        showNotification('Video araması sonlandırıldı', 'info');
        loadActiveVideoCalls();
    } catch (error) {
        showNotification('Video araması sonlandırılamadı', 'error');
    }
}

// ============================================================================
// LIVE STREAM FUNCTIONS
// ============================================================================

async function startStream() {
    const title = document.getElementById('streamTitle').value.trim();
    const description = document.getElementById('streamDescription').value.trim();

    if (!title) {
        showNotification('Yayın başlığı gerekli', 'error');
        return;
    }

    try {
        // 1. Get Camera & Mic
        localStream = await navigator.mediaDevices.getUserMedia({
            video: { width: 1280, height: 720 },
            audio: true
        });

        // 2. Show Preview
        const videoEl = document.getElementById('streamVideo');
        videoEl.srcObject = localStream;
        document.getElementById('streamPreviewContainer').style.display = 'block';

        // 3. Start Recording
        streamChunks = [];
        const options = { mimeType: 'video/webm;codecs=vp9' };
        if (!MediaRecorder.isTypeSupported(options.mimeType)) {
            options.mimeType = 'video/webm';
        }

        streamRecorder = new MediaRecorder(localStream, options);
        streamRecorder.ondataavailable = (e) => {
            if (e.data.size > 0) streamChunks.push(e.data);
        };

        streamRecorder.start(1000);

        // 4. Notify Backend
        const response = await apiFetch(`/api/stream/start`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                streamerId: `user_${currentUser}`,
                streamerName: currentUser,
                title: title,
                description: description || 'Canlı yayın'
            })
        });

        if (response.ok) {
            const data = await response.json();
            currentStreamId = data.id;
            /* showNotification(`Canlı yayın başladı! Stream Key: ${data.streamKey}`, 'success'); */
            showNotification(`Canlı yayın başladı!`, 'success');

            // Toggle Buttons
            document.getElementById('startStreamBtn').style.display = 'none';
            document.getElementById('endStreamBtn').style.display = 'inline-block';

            // Clear inputs
            document.getElementById('streamTitle').value = '';
            document.getElementById('streamDescription').value = '';

            loadLiveStreams();
        }
    } catch (error) {
        console.error(error);
        if (localStream) {
            localStream.getTracks().forEach(t => t.stop());
        }
        showNotification('Yayın başlatılamadı: ' + error.message, 'error');
    }
}

async function endStream() {
    if (!currentStreamId) return;

    try {
        // 1. Stop Recording
        if (streamRecorder && streamRecorder.state !== 'inactive') {
            streamRecorder.stop();
        }

        // 2. Stop Camera
        if (localStream) {
            localStream.getTracks().forEach(track => track.stop());
            document.getElementById('streamVideo').srcObject = null;
        }

        document.getElementById('streamPreviewContainer').style.display = 'none';

        // 3. Notify Backend
        await apiFetch(`/api/stream/end/${currentStreamId}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: '{}'
        });

        // 4. Save Dialog
        setTimeout(async () => {
            if (streamChunks.length > 0) {
                if (confirm('Yayın bitti. Kaydı kaydetmek ister misiniz?')) {
                    await saveStreamRecording();
                }
            }
        }, 500);

        showNotification('Yayın sonlandırıldı', 'info');

        // Reset UI
        document.getElementById('startStreamBtn').style.display = 'inline-block';
        document.getElementById('endStreamBtn').style.display = 'none';
        currentStreamId = null;

        loadLiveStreams();

    } catch (error) {
        console.error(error);
        showNotification('Yayın sonlandırma hatası', 'error');
    }
}

async function saveStreamRecording() {
    const blob = new Blob(streamChunks, { type: 'video/webm' });
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
    const filename = `canli-yayin_${timestamp}.webm`;

    try {
        if ('showSaveFilePicker' in window) {
            const handle = await window.showSaveFilePicker({
                suggestedName: filename,
                types: [{
                    description: 'Video File',
                    accept: { 'video/webm': ['.webm'] }
                }]
            });
            const writable = await handle.createWritable();
            await writable.write(blob);
            await writable.close();
            showNotification('Kayıt başarıyla kaydedildi', 'success');
        } else {
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            a.click();
            URL.revokeObjectURL(url);
        }
    } catch (err) {
        if (err.name !== 'AbortError') {
            showNotification('Kaydetme hatası', 'error');
        }
    }
}

// Track which streams the current user is watching
let watchingStreams = new Set();
// Variables for streaming
let streamRecorder = null;
let streamChunks = [];
let localStream = null;
let currentStreamId = null;

async function loadLiveStreams() {
    try {
        const response = await apiFetch(`/api/stream/live`);
        const streams = await response.json();

        const gridEl = document.getElementById('liveStreams');

        if (streams.length === 0) {
            gridEl.innerHTML = `
                <div class="empty-state">
                    <i class="fas fa-tv"></i>
                    <p>Şu anda canlı yayın yok</p>
                </div>
            `;
        } else {
            gridEl.innerHTML = streams.map(stream => {
                const isWatching = watchingStreams.has(stream.id);
                return `
                    <div class="stream-item">
                        <div class="stream-thumbnail">
                            <i class="fas fa-broadcast-tower"></i>
                            <div class="live-badge">CANLI</div>
                        </div>
                        <div class="stream-info">
                            <h4>${escapeHtml(stream.title)}</h4>
                            <p>${escapeHtml(stream.description)}</p>
                            <div class="stream-meta">
                                <span class="viewer-count">
                                    <i class="fas fa-eye"></i> ${stream.viewerCount} izleyici
                                </span>
                                ${isWatching ? `
                                    <button class="btn btn-sm btn-danger" onclick="leaveStream('${stream.id}')">
                                        <i class="fas fa-sign-out-alt"></i> Ayrıl
                                    </button>
                                ` : `
                                    <button class="btn btn-sm btn-primary" onclick="joinStream('${stream.id}')">
                                        <i class="fas fa-play"></i> İzle
                                    </button>
                                `}
                            </div>
                        </div>
                    </div>
                `;
            }).join('');
        }

        updateBadge('streamBadge', streams.length);
    } catch (error) {
        console.error('Failed to load streams:', error);
    }
}

async function joinStream(streamId) {
    try {
        const response = await apiFetch(`/api/stream/join/${streamId}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                viewerId: `user_${currentUser}`
            })
        });

        if (response.ok) {
            watchingStreams.add(streamId);
            showNotification('Yayına katıldınız', 'success');
            loadLiveStreams();
        }
    } catch (error) {
        showNotification('Yayına katılınamadı', 'error');
    }
}

async function leaveStream(streamId) {
    try {
        const response = await apiFetch(`/api/stream/leave/${streamId}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                viewerId: `user_${currentUser}`
            })
        });

        if (response.ok) {
            watchingStreams.delete(streamId);
            showNotification('Yayından ayrıldınız', 'info');
            loadLiveStreams();
        }
    } catch (error) {
        showNotification('Yayından ayrılınamadı', 'error');
    }
}

/* Replaces old endStream */

// ============================================================================
// SCREEN RECORDING FUNCTIONS
// ============================================================================

let mediaRecorder = null;
let recordedChunks = [];
let recordingStream = null;
let recordingStartTime = null;
let timerInterval = null;
let currentRecordingId = null;

async function startScreenRecording() {
    const quality = document.getElementById('recordingQuality').value;
    const captureType = document.getElementById('captureType').value;
    const filename = document.getElementById('recordingFilename').value || 'ekran-kaydi';
    const includeAudio = document.getElementById('includeAudio').checked;

    try {
        // Set video constraints based on quality
        let videoConstraints = {};
        switch (quality) {
            case '1080p':
                videoConstraints = { width: 1920, height: 1080 };
                break;
            case '720p':
                videoConstraints = { width: 1280, height: 720 };
                break;
            case '480p':
                videoConstraints = { width: 854, height: 480 };
                break;
        }

        // 1. Get Screen Stream (Video + Optional System Audio)
        const displayMediaOptions = {
            video: {
                ...videoConstraints,
                frameRate: 30
            },
            audio: includeAudio // Requests system audio if supported
        };

        let screenStream = await navigator.mediaDevices.getDisplayMedia(displayMediaOptions);

        let finalStream = new MediaStream();
        // Add video track
        screenStream.getVideoTracks().forEach(track => finalStream.addTrack(track));

        // 2. Handle Audio Mixing if requested
        if (includeAudio) {
            try {
                const audioContext = new AudioContext();
                const destination = audioContext.createMediaStreamDestination();
                let hasAudioSource = false;

                // A. Add System Audio if present
                if (screenStream.getAudioTracks().length > 0) {
                    const systemSource = audioContext.createMediaStreamSource(screenStream);
                    const systemGain = audioContext.createGain();
                    systemGain.gain.value = 1.0;
                    systemSource.connect(systemGain).connect(destination);
                    hasAudioSource = true;
                }

                // B. Get Microphone Audio
                try {
                    const micStream = await navigator.mediaDevices.getUserMedia({
                        audio: {
                            echoCancellation: true,
                            noiseSuppression: true,
                            sampleRate: 44100
                        }
                    });

                    if (micStream.getAudioTracks().length > 0) {
                        const micSource = audioContext.createMediaStreamSource(micStream);
                        const micGain = audioContext.createGain();
                        micGain.gain.value = 1.0; // Adjustable
                        micSource.connect(micGain).connect(destination);
                        hasAudioSource = true;

                        // Keep track of mic stream to stop it later
                        recordingStream = new MediaStream([...screenStream.getTracks(), ...micStream.getTracks()]);
                    }
                } catch (micErr) {
                    console.warn('Microphone access denied or not available:', micErr);
                    showNotification('Mikrofon erişimi sağlanamadı, sadece sistem sesi (varsa) kaydedilecek.', 'warning');
                }

                // If we succeeded in adding any audio, use the mixed track
                if (hasAudioSource) {
                    destination.stream.getAudioTracks().forEach(track => finalStream.addTrack(track));
                }

            } catch (audioErr) {
                console.error('Audio mixing error:', audioErr);
                // Fallback: If mixing fails, just try to add whatever audio tracks we have from screen
                screenStream.getAudioTracks().forEach(track => finalStream.addTrack(track));
            }
        } else {
            // If audio not requested, ensure no audio tracks are in final stream (redundant but safe)
        }

        // Store the stream we are using for recording
        // We need to keep 'recordingStream' as the one that holds actual source tracks to stop them later
        // If we mixed audio, recordingStream might need to hold original tracks to stop hardware
        if (!recordingStream) {
            recordingStream = screenStream;
        }

        // Show preview (Video only)
        const previewVideo = document.getElementById('previewVideo');
        previewVideo.srcObject = finalStream;
        // Mute preview to avoid feedback loop
        previewVideo.muted = true;
        document.getElementById('recordingPreview').style.display = 'block';

        // Setup MediaRecorder
        const options = {
            mimeType: 'video/webm;codecs=vp9',
            videoBitsPerSecond: quality === '1080p' ? 8000000 : (quality === '720p' ? 5000000 : 2500000)
        };

        if (!MediaRecorder.isTypeSupported(options.mimeType)) {
            options.mimeType = 'video/webm';
        }

        recordedChunks = [];
        // Use finalStream (Mixed) for recording
        mediaRecorder = new MediaRecorder(finalStream, options);

        mediaRecorder.ondataavailable = (event) => {
            if (event.data.size > 0) {
                recordedChunks.push(event.data);
            }
        };

        mediaRecorder.onstop = async () => {
            // Save recording when MediaRecorder stops
            if (recordedChunks.length > 0) {
                await saveRecording();
            }
        };

        // Notify backend
        const response = await apiFetch(`/api/recording/start`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                userId: `user_${currentUser}`,
                userName: currentUser,
                filename: filename,
                quality: quality,
                captureType: captureType
            })
        });

        if (response.ok) {
            const data = await response.json();
            currentRecordingId = data.id;
        }

        // Start recording
        mediaRecorder.start(1000); // Collect data every second
        recordingStartTime = Date.now();

        // Update UI
        document.getElementById('startRecordingBtn').style.display = 'none';
        document.getElementById('stopRecordingBtn').style.display = 'block';
        document.getElementById('recordingTimer').style.display = 'flex';

        // Start timer
        updateTimer();
        timerInterval = setInterval(updateTimer, 1000);

        showNotification('Ekran kaydı başladı!', 'success');

        // Handle stream end (user clicks "Stop Sharing")
        screenStream.getVideoTracks()[0].onended = () => {
            stopScreenRecording();
        };

    } catch (error) {
        console.error('Screen recording error:', error);
        showNotification('Ekran kaydı başlatılamadı: ' + error.message, 'error');
    }
}

async function stopScreenRecording() {
    let shouldSaveManually = false;

    // Check if mediaRecorder is already inactive (stopped by browser)
    if (mediaRecorder && mediaRecorder.state === 'inactive') {
        shouldSaveManually = true;
    }

    // Stop mediaRecorder if still recording
    if (mediaRecorder && mediaRecorder.state !== 'inactive') {
        mediaRecorder.stop();
        // onstop event will handle saving
    }

    // Stop all tracks
    if (recordingStream) {
        recordingStream.getTracks().forEach(track => track.stop());
    }

    // Clear timer
    if (timerInterval) {
        clearInterval(timerInterval);
        timerInterval = null;
    }

    // Update UI
    document.getElementById('startRecordingBtn').style.display = 'block';
    document.getElementById('stopRecordingBtn').style.display = 'none';
    document.getElementById('recordingTimer').style.display = 'none';
    document.getElementById('recordingPreview').style.display = 'none';

    // Notify backend
    if (currentRecordingId) {
        try {
            const duration = Math.floor((Date.now() - recordingStartTime) / 1000);
            const response = await apiFetch(`/api/recording/stop/${currentRecordingId}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ duration: duration.toString() })
            });

            if (response.ok) {
                const data = await response.json();
                console.log('Recording stopped:', data);
            } else {
                console.error('Failed to stop recording on backend:', await response.text());
            }
        } catch (error) {
            console.error('Error stopping recording:', error);
        }

        // Reset recording ID
        currentRecordingId = null;
    }

    // If browser stopped sharing, manually trigger save
    if (shouldSaveManually && recordedChunks.length > 0) {
        await saveRecording();
    }
}

async function saveRecording() {
    // Prevent multiple calls
    if (recordedChunks.length === 0) {
        console.log('No recorded data to save');
        return;
    }

    const blob = new Blob(recordedChunks, { type: 'video/webm' });

    // Get filename with timestamp
    const filename = document.getElementById('recordingFilename').value || 'ekran-kaydi';
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
    const fullFilename = `${filename}_${timestamp}.webm`;

    try {
        // Try to use File System Access API (Chrome/Edge)
        if ('showSaveFilePicker' in window) {
            const opts = {
                suggestedName: fullFilename,
                types: [{
                    description: 'Video File',
                    accept: { 'video/webm': ['.webm'] }
                }]
            };

            const handle = await window.showSaveFilePicker(opts);
            const writable = await handle.createWritable();
            await writable.write(blob);
            await writable.close();

            showNotification(`Video kaydedildi: ${fullFilename}`, 'success');
        } else {
            // Fallback to regular download
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.style.display = 'none';
            a.href = url;
            a.download = fullFilename;
            document.body.appendChild(a);
            a.click();

            setTimeout(() => {
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
            }, 100);

            showNotification(`Video indiriliyor: ${fullFilename}`, 'success');
        }
    } catch (error) {
        // User cancelled or error occurred
        if (error.name !== 'AbortError') {
            console.error('Save error:', error);
            showNotification('Video kaydedilemedi', 'error');
        }
    }

    // Clear recorded chunks to prevent duplicate saves
    recordedChunks = [];

    // Reload recordings list
    setTimeout(() => {
        loadRecordings();
    }, 500);
}

function updateTimer() {
    if (!recordingStartTime) return;

    const elapsed = Math.floor((Date.now() - recordingStartTime) / 1000);
    const minutes = Math.floor(elapsed / 60);
    const seconds = elapsed % 60;

    const display = `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
    document.getElementById('timerDisplay').textContent = display;
}

async function loadRecordings() {
    try {
        const response = await apiFetch(`/api/recording/list`);
        const recordings = await response.json();

        const listEl = document.getElementById('recordingsList');

        if (recordings.length === 0) {
            listEl.innerHTML = `
                <div class="empty-state">
                    <i class="fas fa-file-video"></i>
                    <p>Henüz kayıt yok</p>
                </div>
            `;
        } else {
            listEl.innerHTML = recordings.map(rec => {
                const duration = rec.duration ? `${Math.floor(rec.duration / 60)}:${String(rec.duration % 60).padStart(2, '0')}` : '00:00';
                return `
                    <div class="recording-item">
                        <div class="recording-item-info">
                            <h4>
                                <i class="fas fa-file-video"></i>
                                ${escapeHtml(rec.filename)}
                                <span class="recording-quality-badge">${rec.quality}</span>
                                <span class="recording-badge ${rec.status}">
                                    ${rec.status === 'recording' ? 'KAYIT EDİLİYOR' : rec.status === 'stopped' ? 'TAMAMLANDI' : rec.status.toUpperCase()}
                                </span>
                            </h4>
                            <p><i class="fas fa-user"></i> ${escapeHtml(rec.userName)}</p>
                            <p><i class="fas fa-clock"></i> ${rec.startTime} - Süre: ${duration}</p>
                            <p><i class="fas fa-desktop"></i> ${getCaptureTypeText(rec.captureType)}</p>
                        </div>
                    </div>
                `;
            }).join('');
        }

        updateBadge('recordingBadge', recordings.filter(r => r.status === 'recording').length);
    } catch (error) {
        console.error('Failed to load recordings:', error);
    }
}

function getCaptureTypeText(type) {
    const types = {
        'screen': 'Tam Ekran',
        'window': 'Pencere',
        'tab': 'Tarayıcı Sekmesi'
    };
    return types[type] || type;
}

// ============================================================================
// STATS FUNCTIONS
// ============================================================================

async function refreshStats() {
    try {
        const response = await apiFetch(`/api/health`);
        const data = await response.json();

        if (data.stats) {
            document.getElementById('statMessages').textContent = data.stats.messages || 0;
            document.getElementById('statVoiceCalls').textContent = data.stats.activeCalls || 0;
            document.getElementById('statVideoCalls').textContent = data.stats.activeCalls || 0;
            document.getElementById('statStreams').textContent = data.stats.liveStreams || 0;

            document.getElementById('miniStatMessages').textContent = data.stats.messages || 0;
            document.getElementById('miniStatCalls').textContent = data.stats.activeCalls || 0;
            document.getElementById('miniStatStreams').textContent = data.stats.liveStreams || 0;

        }

        updateHealthIndicators(true, data);
    } catch (error) {
        console.error('Failed to refresh stats:', error);
        updateHealthIndicators(false);
    }
}

function updateHealthIndicators(isOnline, data = {}) {
    const statusEl = document.getElementById('serverStatus');
    const statusText = isOnline ? 'Çevrimiçi' : 'Çevrimdışı';
    const timestamp = data.timestamp || new Date().toLocaleString('tr-TR');

    if (statusEl) {
        statusEl.classList.toggle('online', isOnline);
        statusEl.classList.toggle('offline', !isOnline);
        statusEl.querySelector('.status-text').textContent = statusText;
    }

    const healthChip = document.getElementById('healthChip');
    if (healthChip) {
        healthChip.classList.toggle('online', isOnline);
        healthChip.classList.toggle('offline', !isOnline);
        const icon = healthChip.querySelector('i');
        if (icon) {
            icon.className = `fas ${isOnline ? 'fa-circle' : 'fa-circle-xmark'}`;
        }
        const text = document.getElementById('healthText');
        if (text) {
            text.textContent = statusText;
        }
    }

    const heroTimestamp = document.getElementById('heroTimestamp');
    if (heroTimestamp) {
        heroTimestamp.textContent = timestamp;
    }

    const lastUpdate = document.getElementById('lastUpdate');
    if (lastUpdate) {
        lastUpdate.textContent = timestamp;
    }

    renderServerInfoGrid(statusText, timestamp, data.service);
}

function renderServerInfoGrid(statusText, timestamp, serviceName = 'Media Server') {
    const serverInfo = document.getElementById('serverInfo');
    if (!serverInfo) return;

    const isOnline = statusText === 'Çevrimiçi';
    serverInfo.innerHTML = `
        <div class="info-item">
            <strong>Durum:</strong>
            <span class="${isOnline ? 'text-success' : 'text-danger'}">${statusText}</span>
        </div>
        <div class="info-item">
            <strong>Servis:</strong>
            <span>${serviceName || 'Media Server'}</span>
        </div>
        <div class="info-item">
            <strong>Adres:</strong>
            <span>${API_BASE}</span>
        </div>
        <div class="info-item">
            <strong>Son Güncelleme:</strong>
            <span id="lastUpdate">${timestamp}</span>
        </div>
    `;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

function updateBadge(badgeId, count) {
    const badge = document.getElementById(badgeId);
    if (badge) {
        badge.textContent = count;
        badge.style.display = count > 0 ? 'inline-block' : 'none';
    }
}

function getStatusText(status) {
    const statusMap = {
        'ringing': 'Çalıyor',
        'active': 'Aktif',
        'ended': 'Bitti'
    };
    return statusMap[status] || status;
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function showNotification(message, type = 'info') {
    const container = document.getElementById('notifications');
    const notification = document.createElement('div');

    const iconMap = {
        'success': 'fa-check-circle',
        'error': 'fa-exclamation-circle',
        'info': 'fa-info-circle'
    };

    notification.className = `notification ${type}`;
    notification.innerHTML = `
        <i class="fas ${iconMap[type]}"></i>
        <span>${message}</span>
    `;

    container.appendChild(notification);

    setTimeout(() => {
        notification.style.animation = 'slideInRight 0.3s reverse';
        setTimeout(() => notification.remove(), 300);
    }, 3000);
}

function startAutoRefresh() {
    // Refresh every 5 seconds
    refreshInterval = setInterval(() => {
        if (!realtimeConnected) {
            loadChatMessages();
            loadActiveVoiceCalls();
            loadActiveVideoCalls();
            loadLiveStreams();
            loadRecordings();
        }
        checkServerHealth();
    }, 5000);
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (refreshInterval) {
        clearInterval(refreshInterval);
    }
    if (reconnectTimer) {
        clearTimeout(reconnectTimer);
    }
    if (realtimeSocket && realtimeSocket.readyState === WebSocket.OPEN) {
        realtimeSocket.close();
    }
});
