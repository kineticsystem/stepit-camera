import { readFileSync, existsSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { previewLocation, rawPreview } from '../src/client/camera/raw';
import { tiff } from './tiff';

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

  it('is located from the start of the file only', () => {
    const file = tiff(jpeg);
    expect(previewLocation(file.subarray(0, 50))).toEqual({ offset: 50, length: jpeg.length });
    expect(previewLocation(file.subarray(0, 20))).toBeUndefined();
  });

  // A real CR2, when one is at hand: RAW_SAMPLE=/path/to/IMG_0001.CR2 pnpm test
  it.runIf(process.env.RAW_SAMPLE && existsSync(process.env.RAW_SAMPLE))('is found in a real CR2', () => {
    const preview = rawPreview(new Uint8Array(readFileSync(process.env.RAW_SAMPLE!)));
    expect(preview?.length).toBeGreaterThan(100000);
  });
});
