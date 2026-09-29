// The preview a Canon RAW file carries, which a browser can show when it
// cannot show the RAW itself.
//
// A CR2 is a TIFF file. Its first image (IFD0) is a JPEG of the whole picture,
// the one the camera shows on its screen, stored as a single strip: tag 0x0111
// gives where it starts in the file, and tag 0x0117 how long it is.

const STRIP_OFFSETS = 0x0111;
const STRIP_BYTE_COUNTS = 0x0117;
const SHORT = 3;
const LONG = 4;

/** The JPEG preview inside a CR2 file, or undefined if there is none. */
export function rawPreview(bytes: Uint8Array): Uint8Array | undefined {
  if (bytes.length < 16) return undefined;
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const order = String.fromCharCode(bytes[0], bytes[1]);
  if (order !== 'II' && order !== 'MM') return undefined;
  const little = order === 'II';
  if (view.getUint16(2, little) !== 42) return undefined;

  const ifd = view.getUint32(4, little);
  if (ifd + 2 > bytes.length) return undefined;
  let offset: number | undefined;
  let length: number | undefined;
  const entries = view.getUint16(ifd, little);
  for (let i = 0; i < entries; i++) {
    const entry = ifd + 2 + i * 12;
    if (entry + 12 > bytes.length) return undefined;
    const tag = view.getUint16(entry, little);
    const type = view.getUint16(entry + 2, little);
    // A single value is stored in the entry itself.
    const value = type === SHORT ? view.getUint16(entry + 8, little) : type === LONG ? view.getUint32(entry + 8, little) : undefined;
    if (tag === STRIP_OFFSETS) offset = value;
    else if (tag === STRIP_BYTE_COUNTS) length = value;
  }
  if (offset === undefined || !length || offset + length > bytes.length) return undefined;
  const jpeg = bytes.subarray(offset, offset + length);
  // A JPEG starts with the marker FF D8.
  return jpeg[0] === 0xff && jpeg[1] === 0xd8 ? jpeg : undefined;
}
