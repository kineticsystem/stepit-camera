import { readFileSync, existsSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { rawPreview } from '../src/client/camera/raw';

/** A TIFF whose first image is the given JPEG, as in a CR2. */
function tiff(jpeg: number[], little = true): Uint8Array {
  const entries: [number, number, number][] = [[0x0100, 4, 5616], [0x0111, 4, 0], [0x0117, 4, jpeg.length]];
  const ifd = 8;
  const data = ifd + 2 + entries.length * 12 + 4;
  entries[1][2] = data;
  const bytes = new Uint8Array(data + jpeg.length);
  const view = new DataView(bytes.buffer);
  bytes.set(little ? [0x49, 0x49] : [0x4d, 0x4d]);
  view.setUint16(2, 42, little);
  view.setUint32(4, ifd, little);
  view.setUint16(ifd, entries.length, little);
  entries.forEach(([tag, type, value], i) => {
    const at = ifd + 2 + i * 12;
    view.setUint16(at, tag, little);
    view.setUint16(at + 2, type, little);
    view.setUint32(at + 4, 1, little);
    view.setUint32(at + 8, value, little);
  });
  bytes.set(jpeg, data);
  return bytes;
}

describe('the preview inside a RAW', () => {
  const jpeg = [0xff, 0xd8, 1, 2, 3, 0xff, 0xd9];

  it('is the JPEG of the first image', () => {
    expect(rawPreview(tiff(jpeg))).toEqual(new Uint8Array(jpeg));
    expect(rawPreview(tiff(jpeg, false))).toEqual(new Uint8Array(jpeg));
  });

  it('is missing from a file that is not a TIFF, or whose first image is not a JPEG', () => {
    expect(rawPreview(new Uint8Array([0xff, 0xd8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]))).toBeUndefined();
    expect(rawPreview(tiff([1, 2, 3]))).toBeUndefined();
    expect(rawPreview(tiff(jpeg).subarray(0, 30))).toBeUndefined();
  });

  // A real CR2, when one is at hand: RAW_SAMPLE=/path/to/IMG_0001.CR2 pnpm test
  it.runIf(process.env.RAW_SAMPLE && existsSync(process.env.RAW_SAMPLE))('is found in a real CR2', () => {
    const preview = rawPreview(new Uint8Array(readFileSync(process.env.RAW_SAMPLE!)));
    expect(preview?.length).toBeGreaterThan(100000);
  });
});
