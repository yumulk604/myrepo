const { app, BrowserWindow } = require('electron');
const path = require('path');
const { spawn } = require('child_process');
const http = require('http');

let mainWindow;
let serverProcess;

// Path to the backend executable (relative to this script)
// Assuming structure: root/launcher/main.js and root/media_server.exe
const serverPath = path.resolve(__dirname, '..', 'media_server.exe');
const serverUrl = 'http://localhost:8080';

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    title: "Media Server Launcher",
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: false,
      contextIsolation: true
    }
  });

  // Load a loading screen or message first
  mainWindow.loadURL(`data:text/html;charset=utf-8,
    <html>
      <body style="font-family: sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; background: #222; color: #fff;">
        <h2>Starting Media Server...</h2>
      </body>
    </html>`);

  // Poll server until ready, then load UI
  checkServerReady();
}

function startServer() {
  console.log(`Starting server from: ${serverPath}`);

  // Set working directory to project root so it finds its assets if any
  const cwd = path.resolve(__dirname, '..');

  serverProcess = spawn(serverPath, [], {
    cwd: cwd,
    stdio: 'pipe', // pipe stdio to log output
    windowsHide: true // Optionally hide console window
  });

  serverProcess.stdout.on('data', (data) => {
    console.log(`[Server]: ${data}`);
  });

  serverProcess.stderr.on('data', (data) => {
    console.error(`[Server Error]: ${data}`);
  });

  serverProcess.on('close', (code) => {
    console.log(`Server process exited with code ${code}`);
    serverProcess = null;
    // If server crashes, maybe quit app or show error?
    if (code !== 0 && !app.isQuitting) {
      // Optional: Show error dialog
    }
  });

  serverProcess.on('error', (err) => {
    console.error('Failed to start server:', err);
  });
}

function checkServerReady() {
  const req = http.get(`${serverUrl}/api/health`, (res) => {
    if (res.statusCode === 200) {
      console.log('Server is ready!');
      mainWindow.loadURL(serverUrl);
    } else {
      setTimeout(checkServerReady, 1000);
    }
  });

  req.on('error', (e) => {
    // Connection refused etc, server not up yet
    setTimeout(checkServerReady, 1000);
  });

  req.end();
}

app.whenReady().then(() => {
  // Handle screen recording requests
  const { session, desktopCapturer, Menu } = require('electron');

  session.defaultSession.setDisplayMediaRequestHandler((request, callback) => {
    desktopCapturer.getSources({ types: ['screen', 'window'] }).then((sources) => {
      const menu = Menu.buildFromTemplate(
        sources.map((source) => ({
          label: source.name,
          click: () => {
            callback({ video: source, audio: 'loopback' });
          },
        }))
      );
      menu.popup();
    });
  });

  startServer();
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('before-quit', () => {
  app.isQuitting = true;
  if (serverProcess) {
    console.log('Killing server process...');
    serverProcess.kill();
  }
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
