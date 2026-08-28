/**
 * zip-unpacker.js — In-memory ZIP (.eenk) decompression using standard Web APIs.
 * Supports STORE (method 0) and DEFLATE (method 8 via DecompressionStream).
 */

async function decompressDeflateRaw(compressedBytes) {
    if (typeof DecompressionStream === 'undefined') {
        throw new Error('Browser does not support DecompressionStream.');
    }
    const ds = new DecompressionStream('deflate-raw');
    const writer = ds.writable.getWriter();
    writer.write(compressedBytes);
    writer.close();

    const reader = ds.readable.getReader();
    const chunks = [];
    let totalLength = 0;

    while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        chunks.push(value);
        totalLength += value.length;
    }

    const result = new Uint8Array(totalLength);
    let offset = 0;
    for (const chunk of chunks) {
        result.set(chunk, offset);
        offset += chunk.length;
    }
    return result;
}

/**
 * Unzips a .eenk or .zip archive from an ArrayBuffer / Uint8Array.
 * @param {ArrayBuffer|Uint8Array} buffer
 * @returns {Promise<Array<{name: string, data: Uint8Array}>>}
 */
async function unpackZip(buffer) {
    const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);

    // 1. Locate End of Central Directory (EOCD) signature: 0x06054B50
    let eocdOffset = -1;
    for (let i = bytes.length - 22; i >= Math.max(0, bytes.length - 65557); i--) {
        if (view.getUint32(i, true) === 0x06054B50) {
            eocdOffset = i;
            break;
        }
    }

    if (eocdOffset === -1) {
        throw new Error('Invalid archive: End of Central Directory not found.');
    }

    const totalEntries = view.getUint16(eocdOffset + 10, true);
    const centralDirSize = view.getUint32(eocdOffset + 12, true);
    const centralDirOffset = view.getUint32(eocdOffset + 16, true);

    const files = [];
    let cdPtr = centralDirOffset;

    for (let i = 0; i < totalEntries; i++) {
        if (view.getUint32(cdPtr, true) !== 0x02014B50) {
            throw new Error(`Corrupted central directory entry at offset ${cdPtr}`);
        }

        const method = view.getUint16(cdPtr + 10, true);
        const compSize = view.getUint32(cdPtr + 20, true);
        const uncompSize = view.getUint32(cdPtr + 24, true);
        const nameLen = view.getUint16(cdPtr + 28, true);
        const extraLen = view.getUint16(cdPtr + 30, true);
        const commentLen = view.getUint16(cdPtr + 32, true);
        const localHeaderOffset = view.getUint32(cdPtr + 42, true);

        const nameBytes = bytes.subarray(cdPtr + 46, cdPtr + 46 + nameLen);
        const filename = new TextDecoder('utf-8').decode(nameBytes);

        // Skip directories
        if (!filename.endsWith('/')) {
            // Read local header to get exact data offset
            if (view.getUint32(localHeaderOffset, true) !== 0x04034B50) {
                throw new Error(`Invalid local header signature for ${filename}`);
            }

            const localNameLen = view.getUint16(localHeaderOffset + 26, true);
            const localExtraLen = view.getUint16(localHeaderOffset + 28, true);
            const dataOffset = localHeaderOffset + 30 + localNameLen + localExtraLen;

            const compressedData = bytes.subarray(dataOffset, dataOffset + compSize);
            let fileData;

            if (method === 0) {
                // Stored (no compression)
                fileData = compressedData.slice();
            } else if (method === 8) {
                // Deflated
                fileData = await decompressDeflateRaw(compressedData);
            } else {
                throw new Error(`Unsupported compression method ${method} in ${filename}`);
            }

            files.push({
                name: filename.replace(/^.*[\\\/]/, ''), // Basename
                fullName: filename,
                data: fileData
            });
        }

        cdPtr += 46 + nameLen + extraLen + commentLen;
    }

    return files;
}

if (typeof window !== 'undefined') {
    window.unpackZip = unpackZip;
}
