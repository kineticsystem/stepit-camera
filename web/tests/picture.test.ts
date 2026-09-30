import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { loadPicture } from '../src/client/camera/picture';
import { tiff } from './tiff';

/** A web server that serves one file, with or without ranges, and records the ranges asked for. */
function serve(file: Uint8Array, ranges = true) {
  const asked: string[] = [];
  vi.stubGlobal('fetch', vi.fn(async (_url: string, init?: RequestInit) => {
    const range = new Headers(init?.headers).get('Range') ?? '';
    asked.push(range);
    const [, from, to] = /bytes=(\d+)-(\d+)/.exec(range)!.map(Number);
    if (!ranges) return new Response(file as BlobPart, { status: 200 });
    const end = Math.min(to, file.length - 1);
    return new Response(file.slice(from, end + 1) as BlobPart, {
      status: 206, headers: { 'Content-Range': `bytes ${from}-${end}/${file.length}` },
    });
  }));
  return asked;
}

describe('a picture from the web server', () => {
  const jpeg = [0xff, 0xd8, 1, 2, 3, 0xff, 0xd9];

  beforeEach(() => vi.stubGlobal('URL', { createObjectURL: () => 'blob:preview' }));
  afterEach(() => vi.unstubAllGlobals());

  it('is a JPEG shown as it is', async () => {
    serve(new Uint8Array([...jpeg, ...new Array(100).fill(0)]));
    await expect(loadPicture('/pictures/IMG_0001.JPG', 'IMG_0001.JPG')).resolves.toEqual({ size: 107, url: '/pictures/IMG_0001.JPG' });
  });

  it('is a RAW shown through its preview, read without the rest of the file', async () => {
    // The preview after the first 64 KB, and more of the RAW after it.
    const padded = tiff([...new Array(70000).fill(0), ...jpeg]);
    const offset = 50 + 70000;
    const file = new Uint8Array(padded.length + 1000);
    file.set(padded);
    // The preview starts at the JPEG, past the zeros.
    new DataView(file.buffer).setUint32(8 + 2 + 12 + 8, offset, true);
    new DataView(file.buffer).setUint32(8 + 2 + 24 + 8, jpeg.length, true);
    const asked = serve(file);

    await expect(loadPicture('/pictures/IMG_0001.CR2', 'IMG_0001.CR2')).resolves.toEqual({ size: file.length, url: 'blob:preview', preview: true });
    expect(asked).toEqual(['bytes=0-65535', `bytes=${offset}-${offset + jpeg.length - 1}`]);
  });

  it('is a RAW whose preview is in the start already read', async () => {
    const asked = serve(tiff(jpeg));
    await expect(loadPicture('/pictures/IMG_0001.CR2', 'IMG_0001.CR2')).resolves.toMatchObject({ url: 'blob:preview', preview: true });
    expect(asked).toHaveLength(1);
  });

  it('is loaded whole from a server without ranges', async () => {
    const file = tiff(jpeg);
    serve(file, false);
    await expect(loadPicture('/pictures/IMG_0001.CR2', 'IMG_0001.CR2')).resolves.toEqual({ size: file.length, url: 'blob:preview', preview: true });
  });

  it('cannot be shown when it is neither a JPEG nor a RAW with a preview', async () => {
    serve(new Uint8Array(100));
    await expect(loadPicture('/pictures/IMG_0001.CR2', 'IMG_0001.CR2')).resolves.toEqual({ size: 100 });
  });

  it('fails when the server does not have it', async () => {
    vi.stubGlobal('fetch', vi.fn(async () => new Response('', { status: 404, statusText: 'Not Found' })));
    await expect(loadPicture('/pictures/IMG_0001.CR2', 'IMG_0001.CR2')).rejects.toThrow('Cannot load /pictures/IMG_0001.CR2: 404 Not Found');
  });
});
