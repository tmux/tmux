/// sentry_level_t
pub const Level = enum(c_int) {
    debug = -1,
    info = 0,
    warning = 1,
    err = 2,
    fatal = 3,
};
