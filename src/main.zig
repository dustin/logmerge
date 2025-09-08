const std = @import("std");
const logmerge = @import("root.zig");

fn output(w: *std.Io.Writer, line: []const u8) !void {
    try w.writeAll(line);
    try w.writeAll("\n");
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer {
        _ = gpa.deinit();
    }
    const alloc = gpa.allocator();
    var args = try std.process.argsWithAllocator(alloc);
    defer args.deinit();
    _ = args.skip();

    var logs = try logmerge.openFiles(alloc, &args);
    defer logs.deinit();

    var writer_buf: [128]u8 = undefined;
    var stdout = std.fs.File.stdout().writer(&writer_buf);
    var w = &stdout.interface;
    defer {
        w.flush() catch {};
    }

    try logs.run(w, output);
}
