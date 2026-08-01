const path = require('path');
const { packImages } = require('../tools/eenky/app/main-process/imagePacker');

async function main() {
    const storyDir = path.resolve(__dirname, 'story');
    const jsonFile = path.join(storyDir, 'story.json');
    const coverFile = 'cover.jpg';
    const thumbnailFile = 'thumbnail.jpg';

    console.log(`[build-test-story] Packing images for test story at ${storyDir}...`);
    await packImages(storyDir, jsonFile, coverFile, thumbnailFile);
    console.log('[build-test-story] Successfully built story.media');
}

main().catch(err => {
    console.error('[build-test-story] Error:', err);
    process.exit(1);
});
