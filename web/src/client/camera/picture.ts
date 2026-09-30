// A picture the driver saved, loaded from the camera's web server for the
// page to show it. A JPEG is shown from the server as it is. A RAW file, e.g.
// 29 MB for a 5D Mark II, is never loaded whole: only its start, to find its
// JPEG preview, and then the preview, with two Range requests.

import { isJpeg, previewLocation } from './raw';

/** How much of the start of a file is read to find the preview of a RAW. It holds the header and the first image's tags. */
const HEAD_SIZE = 64 * 1024;

export interface LoadedPicture {
  /** The size of the whole file, in bytes. */
  size: number;
  /** What the browser can show: the file itself for a JPEG, an object URL of its preview for a RAW, or nothing. */
  url?: string;
  /** The URL shows the preview inside a RAW, not the picture itself. */
  preview?: boolean;
}

export async function loadPicture(file: string, name: string): Promise<LoadedPicture> {
  const head = await fetchRange(file, 0, HEAD_SIZE - 1);
  if (/\.jpe?g$/i.test(name)) return { size: head.size, url: file };

  const at = previewLocation(head.bytes);
  if (!at || at.offset + at.length > head.size) return { size: head.size };
  const jpeg = at.offset + at.length <= head.bytes.length
    ? head.bytes.subarray(at.offset, at.offset + at.length)
    : (await fetchRange(file, at.offset, at.offset + at.length - 1)).bytes;
  if (!isJpeg(jpeg)) return { size: head.size };
  return { size: head.size, url: URL.createObjectURL(new Blob([jpeg as BlobPart], { type: 'image/jpeg' })), preview: true };
}

/** The bytes from start to end, both included, and the size of the whole file. */
async function fetchRange(url: string, start: number, end: number): Promise<{ bytes: Uint8Array; size: number }> {
  const response = await fetch(url, { headers: { Range: `bytes=${start}-${end}` } });
  if (!response.ok) throw new Error(`Cannot load ${url}: ${response.status} ${response.statusText}`.trim());
  const bytes = new Uint8Array(await response.arrayBuffer());
  if (response.status === 206) {
    // e.g. bytes 0-65535/29360128
    const size = Number(/\/(\d+)$/.exec(response.headers.get('Content-Range') ?? '')?.[1]);
    return { bytes, size: Number.isFinite(size) ? size : bytes.length };
  }
  // A server that ignores ranges sends the whole file.
  return { bytes: bytes.subarray(start, end + 1), size: bytes.length };
}
