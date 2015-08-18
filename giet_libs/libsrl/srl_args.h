#ifndef SRL_ARGS_H
#define SRL_ARGS_H



#define SRL_GET_MWMR(port)      (srl_mwmr_t)     APP_GET_ARG(port, VOBJ_TYPE_MWMR)
#define SRL_GET_BARRIER(port)   (srl_barrier_t)  APP_GET_ARG(port, VOBJ_TYPE_BARRIER)
#define SRL_GET_LOCK(port)                       APP_GET_ARG(port, VOBJ_TYPE_LOCK)
#define SRL_GET_MEMSPACE(port)  (srl_memspace_t) APP_GET_ARG(port, VOBJ_TYPE_MEMSPACE)


# define SRL_GET_VBASE(task_name, port, type)                                                      \
({                                                                                                 \
    unsigned int  vbase;                                                                           \
    if (giet_vobj_get_vbase(APP_NAME , args->port, type, &vbase))                                  \
    {                                                                                              \
        srl_log_printf(NONE, "\n[ERROR] in "#task_name" task :\n");                                \
        srl_log_printf(NONE, "          undefined port <"#port"> for channel \"%s\": %x\n",        \
                                                                args->port, vbase);                \
        srl_log_printf(TRACE, "*** &"#port" = %x\n\n", vbase);                                     \
        srl_exit();                                                                                \
    }                                                                                              \
    else                                                                                           \
        srl_log_printf(TRACE, "%s:%d: arg of %s for %s, from %s; &"#port" = %x\n\n",               \
                            __FILE__, __LINE__, APP_NAME, #task_name, #port, vbase);               \
    vbase;                                                                                         \
})


#define SRL_GET_CONST(port)                                                                        \
({                                                                                                 \
    unsigned int vbase;                                                                            \
    if (giet_vobj_get_vbase(APP_NAME, args->port, VOBJ_TYPE_CONST, &vbase)) {                      \
        srl_log_printf(NONE, "\n[ERROR] in some task :\n");                                        \
        srl_log_printf(NONE, "          undefined port <"#port"> for channel \"%s\": %x\n",        \
                                                                args->port, vbase);                \
        srl_log_printf(TRACE, "*** &"#port" = %x\n\n", vbase);                                     \
        srl_exit();                                                                                \
    }                                                                                              \
    else                                                                                           \
        srl_log_printf(TRACE, "%s:%d: arg of %s, from %s; &"#port" = %x\n\n",                      \
                            __FILE__, __LINE__, APP_NAME, #port, vbase);                           \
    *(int *) vbase;                                                                                \
})


#endif
