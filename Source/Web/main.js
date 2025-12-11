// Dolphin Web - Proof of Concept Simulator
// This script simulates the behavior of the Dolphin Emulator running in a web environment.
// Since the actual C++ codebase requires Emscripten to compile to WebAssembly,
// this JS provides a functional frontend that mimics the emulator's core loop for demonstration purposes.

class DolphinWeb {
    constructor() {
        this.canvas = document.getElementById('canvas');
        this.ctx = this.canvas.getContext('2d');
        this.isRunning = false;
        this.fps = 0;
        this.frameCount = 0;
        this.lastTime = 0;
        this.animationId = null;

        this.logElement = document.getElementById('log');
        this.fpsElement = document.getElementById('fps');
        this.statusElement = document.getElementById('status');

        // Simulating some GameCube state
        this.cubeRotation = 0;
        this.bootProgress = 0;
        this.booting = true;
    }

    log(message, type = 'info') {
        const div = document.createElement('div');
        div.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
        div.className = `log-${type}`;
        this.logElement.appendChild(div);
        this.logElement.parentNode.scrollTop = this.logElement.parentNode.scrollHeight;
    }

    init() {
        this.log("Initializing Dolphin Web...", "info");
        this.log("Loading simulated core...", "info");

        setTimeout(() => {
            this.log("Core initialized.", "success");
            this.log("Ready to boot.", "info");
        }, 1000);
    }

    start() {
        if (this.isRunning) return;

        this.isRunning = true;
        this.booting = true;
        this.bootProgress = 0;
        this.statusElement.textContent = "Running";
        this.log("Starting emulation...", "info");

        document.getElementById('overlay').style.opacity = '0';
        document.getElementById('overlay').style.pointerEvents = 'none';

        this.lastTime = performance.now();
        this.loop();
    }

    stop() {
        this.isRunning = false;
        this.statusElement.textContent = "Stopped";
        cancelAnimationFrame(this.animationId);
        document.getElementById('overlay').style.opacity = '1';
        document.getElementById('overlay').style.pointerEvents = 'auto';
    }

    loop() {
        if (!this.isRunning) return;

        const now = performance.now();
        const delta = now - this.lastTime;

        if (delta >= 1000) {
            this.fps = this.frameCount;
            this.frameCount = 0;
            this.lastTime = now;
            this.fpsElement.textContent = this.fps;
        }

        this.update();
        this.draw();

        this.frameCount++;
        this.animationId = requestAnimationFrame(() => this.loop());
    }

    update() {
        // Simulate emulator logic
        if (this.booting) {
            this.bootProgress += 0.5;
            if (this.bootProgress >= 100) {
                this.booting = false;
                this.log("IPL Loaded. Booting GameCube Menu...", "success");
            }
        }

        this.cubeRotation += 0.02;
    }

    draw() {
        // Clear screen
        this.ctx.fillStyle = '#000';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

        if (this.booting) {
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

        // Progress bar
        this.ctx.strokeStyle = '#fff';
        this.ctx.strokeRect(this.canvas.width / 2 - 100, this.canvas.height / 2 + 10, 200, 20);
        this.ctx.fillStyle = '#00a4e4';
        this.ctx.fillRect(this.canvas.width / 2 - 98, this.canvas.height / 2 + 12, 196 * (this.bootProgress / 100), 16);
    }

    drawGameCubeLogo() {
        const cx = this.canvas.width / 2;
        const cy = this.canvas.height / 2;
        const size = 100;

        this.ctx.save();
        this.ctx.translate(cx, cy);
        this.ctx.rotate(this.cubeRotation);
        this.ctx.rotate(Math.sin(this.cubeRotation * 0.5) * 0.5); // Add some wobble

        // Draw a simple 3D-looking cube representation
        this.ctx.strokeStyle = '#6a0dad'; // Purple
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

        // Inner G
        this.ctx.fillStyle = '#ccc';
        this.ctx.font = 'bold 60px Arial';
        this.ctx.textAlign = 'center';
        this.ctx.textBaseline = 'middle';
        this.ctx.fillText("G", 0, 0);

        this.ctx.restore();

        // Draw stats overlay simulated
        this.ctx.fillStyle = "rgba(0, 0, 0, 0.5)";
        this.ctx.fillRect(10, 10, 150, 60);
        this.ctx.fillStyle = "#0f0";
        this.ctx.font = "12px monospace";
        this.ctx.textAlign = "left";
        this.ctx.fillText("VPS: 60.00 (100%)", 20, 30);
        this.ctx.fillText("FPS: " + this.fps.toFixed(2), 20, 45);
        this.ctx.fillText("JIT: 2048 ops/f", 20, 60);
    }
}

// Initialize
const dolphin = new DolphinWeb();
dolphin.init();

document.getElementById('start-btn').addEventListener('click', () => {
    dolphin.start();
});
