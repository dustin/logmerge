const std = @import("std");
const zeit = @import("zeit");
const testing = std.testing;

var monthMap = std.static_string_map.StaticStringMap(zeit.Month).initComptime(.{ .{ "Jan", .jan }, .{ "Feb", .feb }, .{ "Mar", .mar }, .{ "Apr", .apr }, .{ "May", .may }, .{ "Jun", .jun }, .{ "Jul", .jul }, .{ "Aug", .aug }, .{ "Sep", .sep }, .{ "Oct", .oct }, .{ "Nov", .nov }, .{ "Dec", .dec } });

test "monthMapping" {
    const months = [_][]const u8{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    const zms = [_]zeit.Month{ .jan, .feb, .mar, .apr, .may, .jun, .jul, .aug, .sep, .oct, .nov, .dec };
    for (months, zms) |month, ix| {
        try testing.expectEqual(ix, monthMap.get(month));
    }
    try testing.expectEqual(null, monthMap.get("Invalid"));
}

fn parseNum(comptime T: type, seg: []const u8) ?T {
    return std.fmt.parseInt(T, seg, 10) catch |err| {
        std.debug.print("Failed to parse {} from {s}: {}\n", .{ T, seg, err });
        return null;
    };
}

// This is an excessively overspecified timestamp parser

fn parseLogTS(line: []const u8) ?i64 {
    const start = std.mem.indexOf(u8, line, "[") orelse return null;
    const end = start + 27;
    if (end >= line.len) return null;
    if (line[end] != ']') return null;

    const tz_hrs = parseNum(i32, line[start + 22 .. start + 22 + 3]) orelse return null;
    const tz_mins = parseNum(i32, line[start + 25 .. start + 25 + 2]) orelse return null;

    return (zeit.Time{
        .year = parseNum(i32, line[start + 8 .. start + 8 + 4]) orelse return null,
        .month = monthMap.get(line[start + 4 .. start + 4 + 3]) orelse return null,
        .day = parseNum(u5, line[start + 1 .. start + 1 + 2]) orelse return null,
        .hour = parseNum(u5, line[start + 13 .. start + 13 + 2]) orelse return null,
        .minute = parseNum(u6, line[start + 16 .. start + 16 + 2]) orelse return null,
        .second = parseNum(u6, line[start + 19 .. start + 19 + 2]) orelse return null,
        .millisecond = 0,
        .microsecond = 0,
        .nanosecond = 0,
        .offset = (tz_hrs * 3600 + tz_mins * 60),
    }).instant().unixTimestamp();
}

test "parseLogTS" {
    const line = "there's a timestamp in [24/May/2007:22:37:27 +0000] some other log data";
    const result = parseLogTS(line);
    try testing.expectEqual(1180046247, result);
}

const bufSize = 8192;

const Abstraction = union(enum) {
    GZ: std.compress.gzip.Decompressor(std.fs.File.Reader),
    Plain: std.fs.File,

    pub fn reader(self: *Abstraction) std.io.AnyReader {
        return switch (self.*) {
            .GZ => self.GZ.reader().any(),
            .Plain => self.Plain.reader().any(),
        };
    }
};

pub const LogFile = struct {
    filename: []const u8,
    next_timestamp: i64,
    file: ?std.fs.File,
    abstraction: ?Abstraction,
    current: []u8,
    buf: [bufSize]u8,

    pub fn close(self: *LogFile) void {
        if (self.file) |f| {
            f.close();
            self.file = null;
            self.abstraction = null;
        }
    }

    pub fn deinit(self: *LogFile) void {
        self.close();
    }

    pub fn compare(_: void, a: *LogFile, b: *LogFile) std.math.Order {
        return std.math.order(a.next_timestamp, b.next_timestamp);
    }

    fn is_gz(_: *LogFile, f: std.fs.File) bool {
        defer {
            f.seekTo(0) catch {};
        }
        const magic = [_]u8{ 0x1f, 0x8b };
        var reader = f.reader();
        var buf: [2]u8 = undefined;
        _ = reader.readAll(&buf) catch |err| {
            std.debug.print("Failed to read from file: {any}\n", .{err});
            return false;
        };
        return std.meta.eql(buf, magic);
    }

    fn abstract(self: *LogFile, f: std.fs.File) !Abstraction {
        if (self.is_gz(f)) {
            return .{ .GZ = std.compress.gzip.decompressor(f.reader()) };
        }
        return .{ .Plain = f };
    }

    pub fn next(self: *LogFile) !bool {
        if (self.file == null) {
            self.file = try std.fs.cwd().openFile(self.filename, .{});
            errdefer self.close();

            self.abstraction = try self.abstract(self.file.?);
            _ = try self.abstraction.?.reader().readUntilDelimiterOrEof(&self.buf, '\n');
        }

        self.current = try self.abstraction.?.reader().readUntilDelimiterOrEof(&self.buf, '\n') orelse return false;

        if (self.current.len == 0) {
            self.close();
            return false;
        }
        self.next_timestamp = parseLogTS(self.current) orelse return error.BadTimestamp;
        return true;
    }
};

pub fn openLogFile(alloc: std.mem.Allocator, filename: [:0]const u8) !*LogFile {
    const file = try std.fs.cwd().openFile(filename, .{});
    errdefer |err| {
        std.debug.print("Failed to open {s}: {any}\n", .{ filename, err });
        file.close();
    }

    var lf = try alloc.create(LogFile);
    errdefer alloc.destroy(lf);

    lf.filename = filename;
    lf.next_timestamp = 0;
    lf.file = file;
    lf.abstraction = null;
    lf.buf = @as([bufSize]u8, @splat(0));
    lf.current = undefined;

    var abs = try lf.abstract(file);
    const current = abs.reader().readUntilDelimiterOrEof(&lf.buf, '\n') catch |err| {
        std.debug.print("Failed to read from {s}: {any}\n", .{ filename, err });
        return error.BadFile;
    };
    lf.current = current orelse return error.BadFile;

    lf.next_timestamp = parseLogTS(lf.current) orelse return error.BadTimestamp;
    lf.close();

    return lf;
}

pub const LogFiles = struct {
    alloc: std.mem.Allocator,
    files: std.PriorityQueue(*LogFile, void, LogFile.compare),

    pub fn next(self: *LogFiles) ?*LogFile {
        return self.files.removeOrNull();
    }

    pub fn restore(self: *LogFiles, lf: *LogFile) !void {
        if (!try lf.next()) {
            lf.deinit();
            self.alloc.destroy(lf);
            return;
        }
        try self.files.add(lf);
    }

    pub fn deinit(self: *LogFiles) void {
        while (self.files.removeOrNull()) |lf| {
            lf.deinit();
            self.alloc.destroy(lf);
        }
        self.files.deinit();
    }

    pub fn run(self: *LogFiles, ctx: anytype, cb: anytype) anyerror!void {
        while (true) {
            const lf = self.next() orelse break;
            try cb(ctx, lf.current);
            try self.restore(lf);
        }
    }
};

pub fn openFiles(alloc: std.mem.Allocator, filenames: *std.process.ArgIterator) !LogFiles {
    var lfs = LogFiles{
        .alloc = alloc,
        .files = std.PriorityQueue(*LogFile, void, LogFile.compare).init(alloc, {}),
    };
    errdefer lfs.deinit();

    while (filenames.next()) |filename| {
        const lf = openLogFile(alloc, filename) catch |err| switch (err) {
            error.BadFile => continue, // skip this one
            else => return err,
        };
        try lfs.files.add(lf);
    }

    return lfs;
}
