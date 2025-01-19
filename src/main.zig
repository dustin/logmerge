const std = @import("std");
const logmerge = @import("root.zig");

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

    const stdout_file = std.io.getStdOut().writer();
    var bw = std.io.bufferedWriter(stdout_file);
    const stdout = bw.writer();
    defer {
        bw.flush() catch {};
    }

    while (true) {
        const lf = logs.next() orelse break;
        try stdout.writeAll(lf.current);
        try stdout.writeAll("\n");
        try logs.restore(lf);
    }
}
