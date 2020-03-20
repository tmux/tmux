compat_functions += [
    'asprintf',
    'cfmakeraw',
    'closefrom',
    'daemon',
    'explicit_bzero',
    'fgetln',
    'freezero',
    'getdtablecount',
    'getprogname',
    'memmem',
    'reallocarray',
    'recallocarray',
    'setenv',
    'setproctitle',
    'strcasestr',
    'strlcat',
    'strlcpy',
    'strndup',
    'strnlen',
    'strsep',
    'strtonum',
]

foreach function : compat_functions
    macro = 'HAVE_@0@'.format(function.underscorify().to_upper())
    has_function = cc.has_function(function)
    config.set(macro, has_function)
    if not has_function
        compat_apis += function
    endif
endforeach


compat_sources = []

foreach api : compat_apis
    compat_sources += files('@0@.c'.format(api))
endforeach
