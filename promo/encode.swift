// PNG frames → H.264 MP4 with AVFoundation. Usage: enc <framesDir> <out.mp4> <fps>
import AVFoundation
import AppKit

let args = CommandLine.arguments
let dir = URL(fileURLWithPath: args[1]), out = URL(fileURLWithPath: args[2]), fps = Int32(args[3])!
let files = try FileManager.default.contentsOfDirectory(atPath: dir.path).filter { $0.hasSuffix(".png") }.sorted()
let first = NSImage(contentsOf: dir.appendingPathComponent(files[0]))!.cgImage(forProposedRect: nil, context: nil, hints: nil)!
let w = first.width, h = first.height

try? FileManager.default.removeItem(at: out)
let writer = try AVAssetWriter(outputURL: out, fileType: .mp4)
let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
    AVVideoCodecKey: AVVideoCodecType.h264, AVVideoWidthKey: w, AVVideoHeightKey: h,
    AVVideoCompressionPropertiesKey: [AVVideoAverageBitRateKey: 8_000_000, AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel]])
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: [
    kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32ARGB, kCVPixelBufferWidthKey as String: w, kCVPixelBufferHeightKey as String: h])
writer.add(input)
writer.startWriting()
writer.startSession(atSourceTime: .zero)

for (i, f) in files.enumerated() {
    autoreleasepool {
        let img = NSImage(contentsOf: dir.appendingPathComponent(f))!.cgImage(forProposedRect: nil, context: nil, hints: nil)!
        var pb: CVPixelBuffer?
        CVPixelBufferPoolCreatePixelBuffer(nil, adaptor.pixelBufferPool!, &pb)
        CVPixelBufferLockBaseAddress(pb!, [])
        let ctx = CGContext(data: CVPixelBufferGetBaseAddress(pb!), width: w, height: h, bitsPerComponent: 8,
                            bytesPerRow: CVPixelBufferGetBytesPerRow(pb!), space: CGColorSpaceCreateDeviceRGB(),
                            bitmapInfo: CGImageAlphaInfo.noneSkipFirst.rawValue)!
        ctx.draw(img, in: CGRect(x: 0, y: 0, width: w, height: h))
        CVPixelBufferUnlockBaseAddress(pb!, [])
        while !input.isReadyForMoreMediaData { usleep(1000) }
        adaptor.append(pb!, withPresentationTime: CMTime(value: CMTimeValue(i), timescale: fps))
    }
}
input.markAsFinished()
let done = DispatchSemaphore(value: 0)
writer.finishWriting { done.signal() }
done.wait()
print(writer.status == .completed ? "ok \(files.count) frames \(w)x\(h)" : "error \(String(describing: writer.error))")
