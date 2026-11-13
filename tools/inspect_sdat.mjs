/**
 * tools/inspect_sdat.mjs
 * Inspecteur et extracteur de son SDAT Nintendo DS pour Pokémon Platine.
 * Décode fidèlement les Waveforms SWAR (PCM8, PCM16, IMA-ADPCM).
 */

import fs from 'node:fs';
import path from 'node:path';

function readHeader(buf) {
  const magic = buf.subarray(0, 4).toString('ascii');
  if (magic !== 'SDAT') throw new Error(`Magic invalide: ${magic}`);

  return {
    magic,
    fileSize: buf.readUInt32LE(8),
    headerSize: buf.readUInt16LE(12),
    numBlocks: buf.readUInt16LE(14),
    symb: { offset: buf.readUInt32LE(0x10), size: buf.readUInt32LE(0x14) },
    info: { offset: buf.readUInt32LE(0x18), size: buf.readUInt32LE(0x1C) },
    fat:  { offset: buf.readUInt32LE(0x20), size: buf.readUInt32LE(0x24) },
    file: { offset: buf.readUInt32LE(0x28), size: buf.readUInt32LE(0x2C) },
  };
}

function parseFAT(buf, fatOffset) {
  if (fatOffset === 0) return [];
  const count = buf.readUInt32LE(fatOffset + 8);
  const entries = [];
  for (let i = 0; i < count; i++) {
    const entryOff = fatOffset + 12 + i * 16;
    const offset = buf.readUInt32LE(entryOff);
    const size = buf.readUInt32LE(entryOff + 4);
    const magic = buf.subarray(offset, offset + 4).toString('ascii');
    entries.push({ id: i, offset, size, magic });
  }
  return entries;
}

function decodeWaveform(wave) {
  if (wave.length < 12) return null;
  const waveType   = wave[0];
  const loopFlag   = wave[1];
  const sampleRate = wave.readUInt16LE(2);
  const timer      = wave.readUInt16LE(4);
  const loopStart  = wave.readUInt16LE(6);
  const lenWords   = wave.readUInt32LE(8);
  const totalBytes = lenWords * 4;

  const rawPcm = wave.subarray(12, 12 + totalBytes);
  if (rawPcm.length === 0) return null;

  const stepTable = [
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
    50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,
    307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,
    1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,
    5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,
    20350,22385,24623,27086,29794,32767
  ];
  const indexTable = [-1,-1,-1,-1, 2,4,6,8];

  const pcm16 = [];
  if (waveType === 0) {
    // 8-bit PCM unsigned
    for (let i = 0; i < rawPcm.length; i++) {
      pcm16.push((rawPcm[i] - 128) * 256);
    }
  } else if (waveType === 1) {
    // 16-bit PCM signed
    for (let i = 0; i + 1 < rawPcm.length; i += 2) {
      pcm16.push(rawPcm.readInt16LE(i));
    }
  } else if (waveType === 2) {
    // IMA-ADPCM 4-bit
    if (rawPcm.length >= 4) {
      let predictor = rawPcm.readInt16LE(0);
      let stepIdx = Math.max(0, Math.min(88, rawPcm[2]));
      const adpcm = rawPcm.subarray(4);
      for (let i = 0; i < adpcm.length; i++) {
        const byte = adpcm[i];
        for (let nibble = 0; nibble < 2; nibble++) {
          const n = (nibble === 0) ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
          const step = stepTable[stepIdx];
          let diff = step >> 3;
          if (n & 1) diff += step >> 2;
          if (n & 2) diff += step >> 1;
          if (n & 4) diff += step;
          if (n & 8) diff = -diff;
          predictor = Math.max(-32768, Math.min(32767, predictor + diff));
          pcm16.push(predictor);
          stepIdx = Math.max(0, Math.min(88, stepIdx + indexTable[n & 7]));
        }
      }
    }
  }

  return {
    sampleRate: sampleRate > 0 ? sampleRate : 22050,
    loop: loopFlag !== 0,
    samples: Int16Array.from(pcm16)
  };
}

function parseSWAR(swar) {
  if (swar.subarray(0, 4).toString('ascii') !== 'SWAR') return [];
  // Number of samples at offset 0x38
  if (swar.length < 0x3C) return [];
  const numSamples = swar.readUInt32LE(0x38);
  const waveforms = [];

  for (let i = 0; i < numSamples; i++) {
    const waveOff = swar.readUInt32LE(0x3C + i * 4);
    if (waveOff === 0 || waveOff >= swar.length) continue;
    const waveData = swar.subarray(waveOff);
    const decoded = decodeWaveform(waveData);
    if (decoded && decoded.samples.length > 0) {
      waveforms.push(decoded);
    }
  }
  return waveforms;
}

function createWavBuffer(samples, sampleRate, channels = 1) {
  const dataSize = samples.length * 2;
  const buf = Buffer.alloc(44 + dataSize);
  buf.write('RIFF', 0);
  buf.writeUInt32LE(36 + dataSize, 4);
  buf.write('WAVE', 8);
  buf.write('fmt ', 12);
  buf.writeUInt32LE(16, 16);
  buf.writeUInt16LE(1, 20); // PCM
  buf.writeUInt16LE(channels, 22);
  buf.writeUInt32LE(sampleRate, 24);
  buf.writeUInt32LE(sampleRate * channels * 2, 28);
  buf.writeUInt16LE(channels * 2, 32);
  buf.writeUInt16LE(16, 34);
  buf.write('data', 36);
  buf.writeUInt32LE(dataSize, 40);

  for (let i = 0; i < samples.length; i++) {
    buf.writeInt16LE(samples[i], 44 + i * 2);
  }
  return buf;
}

function inspectSDAT(filePath, extractDir = null, maxSamples = 20) {
  console.log(`\n=== Inspection de ${filePath} ===`);
  const buf = fs.readFileSync(filePath);
  const hdr = readHeader(buf);
  console.log(`Taille SDAT: ${hdr.fileSize} octets`);

  const fatEntries = parseFAT(buf, hdr.fat.offset);
  const swarEntries = fatEntries.filter(e => e.magic === 'SWAR');
  console.log(`FAT: ${fatEntries.length} entrées (${swarEntries.length} SWAR archives)`);

  if (extractDir && swarEntries.length > 0) {
    fs.mkdirSync(extractDir, { recursive: true });
    let totalExtracted = 0;

    for (let swarIdx = 0; swarIdx < swarEntries.length; swarIdx++) {
      if (totalExtracted >= maxSamples) break;
      const swar = swarEntries[swarIdx];
      const swarData = buf.subarray(swar.offset, swar.offset + swar.size);
      const waveforms = parseSWAR(swarData);
      waveforms.forEach((wf, waveIdx) => {
        if (totalExtracted >= maxSamples) return;
        const wav = createWavBuffer(wf.samples, wf.sampleRate, 1);
        const fileName = path.join(extractDir, `sound_${String(totalExtracted).padStart(4, '0')}.wav`);
        fs.writeFileSync(fileName, wav);
        totalExtracted++;
      });
    }
    console.log(`✓ ${totalExtracted} fichiers WAV extraits dans ${extractDir}`);
  }
}

const args = process.argv.slice(2);
const sdatPath = args[0] || 'psp-assets/data/sound/sound_data.sdat';
const extractDir = args[1] || 'psp-assets/audio_wav';
inspectSDAT(sdatPath, extractDir, 30);
