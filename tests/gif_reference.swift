// Optional independent check of generated first-frame RGB using macOS ImageIO.
import Foundation
import ImageIO
import CoreGraphics

precondition(CommandLine.arguments.count == 2)
let directory = URL(fileURLWithPath: CommandLine.arguments[1])
let files = try FileManager.default.contentsOfDirectory(at: directory,
    includingPropertiesForKeys: nil).filter { $0.pathExtension == "gif" }
precondition(!files.isEmpty)
for file in files.sorted(by: { $0.path < $1.path }) {
    let expected = [UInt8](try Data(contentsOf: file.deletingPathExtension().appendingPathExtension("rgb")))
    func word(_ offset: Int) -> Int { Int(expected[offset]) | Int(expected[offset + 1]) << 8 }
    let width = word(0), height = word(2)
    let source = CGImageSourceCreateWithURL(file as CFURL, nil)!
    let image = CGImageSourceCreateImageAtIndex(source, 0, nil)!
    precondition(image.width == width && image.height == height)
    var pixels = [UInt8](repeating: 0, count: width * height * 4)
    let colorSpace = CGColorSpace(name: CGColorSpace.sRGB)!
    pixels.withUnsafeMutableBytes { bytes in
        let context = CGContext(data: bytes.baseAddress, width: width, height: height,
            bitsPerComponent: 8, bytesPerRow: width * 4, space: colorSpace,
            bitmapInfo: CGBitmapInfo.byteOrder32Big.rawValue | CGImageAlphaInfo.premultipliedLast.rawValue)!
        context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
    }
    for index in 0..<(width * height) {
        for channel in 0..<3 {
            precondition(pixels[index * 4 + channel] == expected[16 + index * 3 + channel],
                         "ImageIO RGB mismatch: \(file.lastPathComponent), pixel \(index), channel \(channel)")
        }
        precondition(pixels[index * 4 + 3] == 255)
    }
}
print("ImageIO agrees with independent first-frame RGB for \(files.count) generated GIFs.")
