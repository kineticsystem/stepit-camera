// The preview a Canon RAW file carries, which a browser can show when it
// cannot show the RAW itself.
//
// A CR2 is a TIFF file. Its first image (IFD0) is a JPEG of the whole picture,
// the one the camera shows on its screen, stored as a single strip: tag 0x0111
// gives where it starts in the file, and tag 0x0117 how long it is. Both are
// near the start of the file, so the preview can be found without reading the
// rest of it: see previewLocation().

const STRIP_OFFSETS = 0x0111;
const STRIP_BYTE_COUNTS = 0x0117;
const SHORT = 3;
const LONG = 4;

/** Where the preview is in the file. */
export interface Location {
  offset: number;
  length: number;
}

/**
 * Where the JPEG preview of a CR2 file is, read from the start of the file,
 * or undefined if it is not a TIFF, or the start given is too short to tell.
 */
export function previewLocation(head: Uint8Array): Location | undefined {
  if (head.length < 16) return undefined;
  const view = new DataView(head.buffer, head.byteOffset, head.byteLength);
  const order = String.fromCharCode(head[0], head[1]);
  if (order !== 'II' && order !== 'MM') return undefined;
  const little = order === 'II';
  if (view.getUint16(2, little) !== 42) return undefined;

  const ifd = view.getUint32(4, little);
  if (ifd + 2 > head.length) return undefined;
  let offset: number | undefined;
  let length: number | undefined;
  const entries = view.getUint16(ifd, little);
  for (let i = 0; i < entries; i++) {
    const entry = ifd + 2 + i * 12;
    if (entry + 12 > head.length) return undefined;
    const tag = view.getUint16(entry, little);
    const type = view.getUint16(entry + 2, little);
    // A single value is stored in the entry itself.
    const value = type === SHORT ? view.getUint16(entry + 8, little) : type === LONG ? view.getUint32(entry + 8, little) : undefined;
    if (tag === STRIP_OFFSETS) offset = value;
    else if (tag === STRIP_BYTE_COUNTS) length = value;
  }
  return offset === undefined || !length ? undefined : { offset, length };
}

/** Whether the bytes are a JPEG image, which starts with the marker FF D8. */
export function isJpeg(bytes: Uint8Array): boolean {
  return bytes[0] === 0xff && bytes[1] === 0xd8;
}

/** The JPEG preview inside a whole CR2 file, or undefined if there is none. */
export function rawPreview(bytes: Uint8Array): Uint8Array | undefined {
  const at = previewLocation(bytes);
  if (!at || at.offset + at.length > bytes.length) return undefined;
  const jpeg = bytes.subarray(at.offset, at.offset + at.length);
  return isJpeg(jpeg) ? jpeg : undefined;
}
