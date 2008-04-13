
/**
 * get_inputs
 * @s: the source handle from which a command will be retrieved
 * @command: the string pointer to receive the retrieved command.
 *
 * The string returned must be deallocated by the caller.
 * Returns: true if more commands are available, false if this is the last command available
 */
extern int get_inputs(void **s, char **command);

/**
 * setup_inputs
 * @dev: the #vfi_dev API handle in use
 * @src: the #vif_source handle to be returned
 * @opts: the options handle in use from gengetopt.
 *
 * This function returns a #vfi_source handle in @src which will
 * return command line parameters. This application uses gengetopt to
 * generate code to handle application parameters and options and this
 * function and the #get_inputs function which it inserts in the @src
 * handle are designed to use this structure. See gengetopts
 * documentation for details.
 * Returns 0 on success or else a negative error code.
 */
extern int setup_inputs(struct vfi_dev *dev, struct vfi_source **src, struct gengetopt_args_info *opts);

/**
 * show_function
 *
 * A sample internal function of the sort which can be invoked by the
 * pipe API pre-command in conjunction with the source processing loop
 * thread. This function is polymorphic in that it parses its
 * signature to determine its input/output parameters. In reality real
 * functions would at most simply check their signature as a debug
 * reality check before processing their fixed function.
 * Returns: 0 on error to terminate the pipeline processing loop or 1 to
 * continue pipeline.
 */
extern int show_function(void **e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result);

/* A sample internal function of the sort which can be invoked by the
 * pipe API pre-command in conjunction with the source processing loop
 * thread. This function is polymorphic in that it parses its
 * signature to determine its input/output parameters. This function is useful in test scripts. */
extern int copy_function(void **e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result);

extern void *source_thread(void *source);

extern int process_commands(pthread_t *tid, struct vfi_source *src, struct gengetopt_args_info *opts);

extern void *driver_thread(void *h);
extern int initialize_api_commands(struct vfi_dev *dev);
