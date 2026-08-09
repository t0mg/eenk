/**
 * device-manager/serial-protocol.js
 *
 * Extracted verbatim from tools/eenky/app/renderer/src/core/serialProtocol.js
 * Single change: removed ES module `export` keyword → plain global class
 * so it loads as a <script> tag with no bundler required.
 *
 * Protocol: line-based text commands + binary payload over USB Serial at 115200 baud.
 * CRC32-LE (polynomial 0xEDB88320) used for transfer integrity verification.
 */

// ── CRC32-LE (matches ESP32 <rom/crc.h> implementation) ──────────────────
const makeCRCTable = function() {
    let c;
    const crcTable = [];
    for (let n = 0; n < 256; n++) {
        c = n;
        for (let k = 0; k < 8; k++) {
            c = ((c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1));
        }
        crcTable[n] = c;
    }
    return crcTable;
};
const crcTable = makeCRCTable();

const crc32_le = function(crc, buf) {
    crc = crc ^ (-1);
    for (let i = 0; i < buf.length; i++) {
        crc = (crc >>> 8) ^ crcTable[(crc ^ buf[i]) & 0xFF];
    }
    return (crc ^ (-1)) >>> 0;
};

// ── Protocol class (global, no export) ───────────────────────────────────
class EenkSerialProtocol {
    constructor() {
        this.port = null;
        this.reader = null;
        this.writer = null;
        this._readBuffer = new Uint8Array(0);
    }

    async connect() {
        try {
            this.port = await navigator.serial.requestPort();
            await this.port.open({ baudRate: 115200 });

            // Allow time for device to restart or clear buffers
            await new Promise(resolve => setTimeout(resolve, 100));
            this._readBuffer = new Uint8Array(0);

            // Send sync multiple times — ESP32 resets on Web Serial connection
            // and takes a couple of seconds to boot and enter MENU mode.
            for (let i = 0; i < 15; i++) {
                await this._writeLine('EENK_SYNC');
                try {
                    const response = await this._readLine(500);
                    if (response.startsWith('OK EENK')) {
                        const parts = response.split(' ');
                        const version  = parts[2] || '1';
                        const freeBytes = parseInt(parts[3] || '0', 10);
                        this._readBuffer = new Uint8Array(0);
                        return { version, freeBytes };
                    } else {
                        console.log('Device said:', response);
                    }
                } catch (timeout) {
                    // ignore, retry
                }
            }
            throw new Error('Failed to synchronize with device after multiple attempts.');
        } catch (e) {
            await this.disconnect();
            throw e;
        }
    }

    async disconnect() {
        if (this.writer) {
            try { await this._writeLine('DISCONNECT'); this.writer.releaseLock(); } catch (_) {}
            this.writer = null;
        }
        if (this.reader) {
            try { await this.reader.cancel(); this.reader.releaseLock(); } catch (_) {}
            this.reader = null;
        }
        if (this.port) {
            try { await this.port.close(); } catch (_) {}
            this.port = null;
        }
        this._readBuffer = new Uint8Array(0);
    }

    async listFiles(path) {
        await this._writeLine(`LIST ${path}`);
        const files = [];
        while (true) {
            const line = await this._readLine();
            if (line === 'END') break;
            if (line.startsWith('ERR')) throw new Error(line);
            const parts = line.split(' ');
            if (parts[0] === 'FILE' && parts.length >= 4) {
                files.push({ type: parts[1], size: parseInt(parts[2], 10), name: parts.slice(3).join(' ') });
            }
        }
        return files;
    }

    async deleteFile(path) {
        await this._writeLine(`DELETE ${path}`);
        const response = await this._readLine();
        if (!response.startsWith('OK')) throw new Error(response);
    }

    async uploadFile(path, data, onProgress) {
        const size = data.length;
        await this._writeLine(`UPLOAD ${path} ${size}`);
        const response = await this._readLine();
        if (!response.startsWith('OK READY')) throw new Error(`Device not ready: ${response}`);

        const chunkSize = 128; // Prevents ESP32 USB CDC RX buffer overflow
        let runningCrc = 0;

        for (let i = 0; i < size; i += chunkSize) {
            const chunk = data.slice(i, i + chunkSize);
            await this._writeLine(`CHUNK ${chunk.length}`);
            await this._writeBytes(chunk);
            runningCrc = crc32_le(runningCrc, chunk);
            const ack = await this._readLine();
            if (!ack.startsWith('OK CHUNK')) throw new Error(`Chunk upload failed: ${ack}`);
            if (onProgress) onProgress(i + chunk.length, size);
        }

        const crcHex = (runningCrc >>> 0).toString(16).padStart(8, '0');
        await this._writeLine(`END ${crcHex}`);
        const finalResp = await this._readLine(5000);
        if (!finalResp.startsWith('OK UPLOAD')) throw new Error(`Upload failed: ${finalResp}`);
    }

    async downloadFile(path, onProgress) {
        await this._writeLine(`DOWNLOAD ${path}`);
        const response = await this._readLine();
        if (response.startsWith('ERR')) throw new Error(response);
        const parts = response.split(' ');
        if (parts[0] !== 'OK' || parts[1] !== 'DOWNLOAD') throw new Error(`Unexpected: ${response}`);

        const size = parseInt(parts[2], 10);
        const data = new Uint8Array(size);
        let received = 0, runningCrc = 0;

        while (received < size) {
            const line = await this._readLine();
            if (line.startsWith('ERR')) throw new Error(line);
            if (!line.startsWith('CHUNK')) throw new Error(`Expected CHUNK, got: ${line}`);
            const chunkSize = parseInt(line.split(' ')[1], 10);
            const chunk = await this._readBytes(chunkSize);
            data.set(chunk, received);
            runningCrc = crc32_le(runningCrc, chunk);
            received += chunkSize;
            if (onProgress) onProgress(received, size);
        }

        const endLine = await this._readLine();
        if (!endLine.startsWith('END')) throw new Error(`Expected END, got: ${endLine}`);
        const expectedCrc = parseInt(endLine.split(' ')[1], 16);
        if (runningCrc !== expectedCrc) {
            throw new Error(`CRC mismatch: expected ${expectedCrc.toString(16)}, got ${runningCrc.toString(16)}`);
        }
        return data;
    }

    async getInfo() {
        this._readBuffer = new Uint8Array(0);
        await this._writeLine('INFO');
        const response = await this._readLine();
        if (response.startsWith('ERR')) throw new Error(response);
        const parts = response.split(' ');
        if (parts[0] === 'OK' && parts[1] === 'INFO' && parts.length >= 5) {
            return { total: parseInt(parts[2], 10), used: parseInt(parts[3], 10), free: parseInt(parts[4], 10) };
        }
        return { total: 0, used: 0, free: 0 };
    }

    async mkdir(path) {
        await this._writeLine(`MKDIR ${path}`);
        const response = await this._readLine();
        if (!response.startsWith('OK')) throw new Error(`mkdir failed: ${response}`);
    }

    // ── Internal helpers ─────────────────────────────────────────────────

    async _getReader() {
        if (!this.reader) this.reader = this.port.readable.getReader();
        return this.reader;
    }

    async _getWriter() {
        if (!this.writer) this.writer = this.port.writable.getWriter();
        return this.writer;
    }

    async _readLine(timeoutMs = 10000) {
        const timeoutPromise = new Promise((_, reject) =>
            setTimeout(() => reject(new Error('Read timeout')), timeoutMs)
        );
        const readPromise = async () => {
            while (true) {
                const newlineIdx = this._readBuffer.indexOf(10);
                if (newlineIdx !== -1) {
                    const lineBytes = this._readBuffer.slice(0, newlineIdx);
                    this._readBuffer = this._readBuffer.slice(newlineIdx + 1);
                    let line = new TextDecoder().decode(lineBytes);
                    if (line.endsWith('\r')) line = line.slice(0, -1);
                    return line;
                }
                const reader = await this._getReader();
                const { value, done } = await reader.read();
                if (done) { this.reader.releaseLock(); this.reader = null; throw new Error('Serial port closed'); }
                if (value) {
                    const newBuf = new Uint8Array(this._readBuffer.length + value.length);
                    newBuf.set(this._readBuffer);
                    newBuf.set(value, this._readBuffer.length);
                    this._readBuffer = newBuf;
                }
            }
        };
        return Promise.race([readPromise(), timeoutPromise]);
    }

    async _readBytes(count, timeoutMs = 10000) {
        const timeoutPromise = new Promise((_, reject) =>
            setTimeout(() => reject(new Error('Read timeout')), timeoutMs)
        );
        const readPromise = async () => {
            while (this._readBuffer.length < count) {
                const reader = await this._getReader();
                const { value, done } = await reader.read();
                if (done) { this.reader.releaseLock(); this.reader = null; throw new Error('Serial port closed'); }
                if (value) {
                    const newBuf = new Uint8Array(this._readBuffer.length + value.length);
                    newBuf.set(this._readBuffer);
                    newBuf.set(value, this._readBuffer.length);
                    this._readBuffer = newBuf;
                }
            }
            const result = this._readBuffer.slice(0, count);
            this._readBuffer = this._readBuffer.slice(count);
            return result;
        };
        return Promise.race([readPromise(), timeoutPromise]);
    }

    async _writeLine(text) {
        await this._writeBytes(new TextEncoder().encode(text + '\n'));
    }

    async _writeBytes(data) {
        const writer = await this._getWriter();
        await writer.write(data);
    }
}
