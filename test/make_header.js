const fs = require('fs');
const path = require('path');
const { packImages } = require('../tools/eenky/app/main-process/imagePacker');

const dummyJson = path.join(__dirname, 'dummy.json');
fs.writeFileSync(dummyJson, '{}');

packImages(__dirname, dummyJson, 'cover.png').then(() => {
    const sidecar = fs.readFileSync(path.join(__dirname, 'main.media'));
    
    let headerStr = `// Auto-generated\n`;
    headerStr += `#pragma once\n`;
    headerStr += `const unsigned char test_sleep_cover_media[] = {\n`;
    for(let i = 0; i < sidecar.length; i++) {
        headerStr += `0x${sidecar[i].toString(16).padStart(2, '0')}, `;
        if((i+1) % 12 === 0) headerStr += `\n`;
    }
    headerStr += `\n};\n`;
    headerStr += `const unsigned int test_sleep_cover_media_len = ${sidecar.length};\n`;
    
    fs.writeFileSync(path.join(__dirname, 'test_sleep_cover_media.h'), headerStr); 
    
    fs.unlinkSync(dummyJson);
    fs.unlinkSync(path.join(__dirname, 'main.media'));
    console.log("Generated test_sleep_cover_media.h");
}).catch(err => {
    console.error(err);
});
