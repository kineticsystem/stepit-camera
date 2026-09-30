// A small CR2-like file, built by the tests.

/** A TIFF whose first image is the given JPEG, as in a CR2. */
export function tiff(jpeg: number[], little = true): Uint8Array {
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
