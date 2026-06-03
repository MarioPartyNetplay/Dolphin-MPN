// Dolphin Web - Frontend
// This script handles the integration with the Emscripten-compiled WASM module.
// It also includes a fallback simulator for demonstration purposes when the WASM is not present.

// -------------------------------------------------------------------------
// Emscripten Module Definition
// -------------------------------------------------------------------------
var Module = {
    preRun: [],
    postRun: [],
    print: function(text) {
        if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
        console.log(text);
        if (window.dolphinApp) window.dolphinApp.log(text, 'info');
    },
    printErr: function(text) {
        if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
        console.error(text);
        if (window.dolphinApp) window.dolphinApp.log(text, 'error');
    },
    canvas: (function() {
        var canvas = document.getElementById('canvas');
        canvas.addEventListener("webglcontextlost", function(e) { alert('WebGL context lost. You will need to reload the page.'); e.preventDefault(); }, false);
        return canvas;
    })(),
    setStatus: function(text) {
        if (window.dolphinApp) window.dolphinApp.setStatus(text);
    },
    totalDependencies: 0,
    monitorRunDependencies: function(left) {
        this.totalDependencies = Math.max(this.totalDependencies, left);
        Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
    }
};

// -------------------------------------------------------------------------
// Application Logic
// -------------------------------------------------------------------------
class DolphinWeb {
    constructor() {
        this.canvas = document.getElementById('canvas');
        this.ctx = this.canvas.getContext('2d'); // Will be WebGL in real app, 2d in simulator
        this.logElement = document.getElementById('log');
        this.fpsElement = document.getElementById('fps');
        this.statusElement = document.getElementById('status');
        this.fileInput = document.getElementById('file-input');
        this.startBtn = document.getElementById('start-btn');

        this.selectedFile = null;
        this.isWasmLoaded = typeof FS !== 'undefined';

        // Simulator state
        this.isRunning = false;
        this.simBootProgress = 0;
        this.simCubeRotation = 0;
        this.animationId = null;

        this.bindEvents();
        this.checkWasmStatus();
    }

    bindEvents() {
        this.fileInput.addEventListener('change', (e) => {
            if (e.target.files.length > 0) {
                this.selectedFile = e.target.files[0];
                this.startBtn.disabled = false;
                this.log(`Selected file: ${this.selectedFile.name} (${(this.selectedFile.size / 1024 / 1024).toFixed(2)} MB)`, 'info');
            } else {
                this.startBtn.disabled = true;
                this.selectedFile = null;
            }
        });

        this.startBtn.addEventListener('click', () => {
            this.bootGame();
        });
    }

    checkWasmStatus() {
        if (this.isWasmLoaded) {
            this.log("WASM Runtime detected. Ready to boot.", "success");
        } else {
            this.log("WASM Runtime not detected. Running in Simulation Mode.", "warn");
            this.log("To run actual games, compile Dolphin to WASM and load this page via a web server.", "info");
        }
    }

    log(message, type = 'info') {
        const div = document.createElement('div');
        div.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
        div.className = `log-${type}`;
        this.logElement.appendChild(div);
        this.logElement.parentNode.scrollTop = this.logElement.parentNode.scrollHeight;
    }

    setStatus(text) {
        this.statusElement.textContent = text;
    }

    async bootGame() {
        if (!this.selectedFile) return;

        document.getElementById('overlay').style.opacity = '0';
        document.getElementById('overlay').style.pointerEvents = 'none';

        if (this.isWasmLoaded) {
            this.log("Loading file into virtual filesystem...", "info");
            try {
                const data = await this.readFileAsArrayBuffer(this.selectedFile);
                // Create file in Emscripten VFS
                const fileName = '/' + this.selectedFile.name;
                FS.writeFile(fileName, new Uint8Array(data));
                this.log(`File written to VFS: ${fileName}`, "success");

                // Boot Dolphin
                this.log("Calling main()...", "info");
                Module.callMain(['-p', 'web', fileName]);
            } catch (e) {
                this.log(`Error booting: ${e}`, "error");
                document.getElementById('overlay').style.opacity = '1';
                document.getElementById('overlay').style.pointerEvents = 'auto';
            }
        } else {
            this.startSimulator();
        }
    }

    readFileAsArrayBuffer(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = () => resolve(reader.result);
            reader.onerror = () => reject(reader.error);
            reader.readAsArrayBuffer(file);
        });
    }

    // -------------------------------------------------------------------------
    // Simulator Code (Fallback)
    // -------------------------------------------------------------------------
    startSimulator() {
        this.isRunning = true;
        this.simBootProgress = 0;
        this.setStatus("Running (Simulation)");
        this.log(`[SIM] Booting ${this.selectedFile.name}...`, "info");

        // Use 2D context for simulator if WebGL not active
        // Note: In real app, canvas would be WebGL. We might need to replace canvas or handle context properly.
        // For PoC simplicity, we assume 2D if WASM missing.
        this.ctx = this.canvas.getContext('2d');

        this.loop();
    }

    loop() {
        if (!this.isRunning) return;

        this.update();
        this.draw();
        this.animationId = requestAnimationFrame(() => this.loop());
    }

    update() {
        this.simBootProgress += 0.5;
        this.simCubeRotation += 0.02;
        if (this.simBootProgress >= 100 && this.simBootProgress < 100.5) {
            this.log("[SIM] IPL Loaded. Starting game...", "success");
        }

        // Update FPS
        this.fpsElement.textContent = "60";
    }

    draw() {
        this.ctx.fillStyle = '#000';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

        if (this.simBootProgress < 100) {
            this.drawBootSequence();
        } else {
            this.drawGameCubeLogo();
        }
    }

    drawBootSequence() {
        this.ctx.fillStyle = '#fff';
        this.ctx.font = '20px monospace';
        this.ctx.textAlign = 'center';
        this.ctx.fillText("Dolphin OS", this.canvas.width / 2, this.canvas.height / 2 - 20);

        this.ctx.strokeStyle = '#fff';
        this.ctx.strokeRect(this.canvas.width / 2 - 100, this.canvas.height / 2 + 10, 200, 20);
        this.ctx.fillStyle = '#00a4e4';
        this.ctx.fillRect(this.canvas.width / 2 - 98, this.canvas.height / 2 + 12, 196 * (this.simBootProgress / 100), 16);
    }

    drawGameCubeLogo() {
        const cx = this.canvas.width / 2;
        const cy = this.canvas.height / 2;
        const size = 100;

        this.ctx.save();
        this.ctx.translate(cx, cy);
        this.ctx.rotate(this.simCubeRotation);
        this.ctx.rotate(Math.sin(this.simCubeRotation * 0.5) * 0.5);

        this.ctx.strokeStyle = '#6a0dad';
        this.ctx.lineWidth = 5;
        this.ctx.fillStyle = 'rgba(106, 13, 173, 0.5)';

        this.ctx.beginPath();
        this.ctx.moveTo(-size/2, -size/2);
        this.ctx.lineTo(size/2, -size/2);
        this.ctx.lineTo(size/2, size/2);
        this.ctx.lineTo(-size/2, size/2);
        this.ctx.closePath();
        this.ctx.stroke();
        this.ctx.fill();

        this.ctx.fillStyle = '#ccc';
        this.ctx.font = 'bold 60px Arial';
        this.ctx.textAlign = 'center';
        this.ctx.textBaseline = 'middle';
        this.ctx.fillText("G", 0, 0);

        this.ctx.restore();
    }
}

// Initialize
window.addEventListener('load', () => {
    window.dolphinApp = new DolphinWeb();
});
