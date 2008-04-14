
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
 * @src: the #vfi_source handle to be returned
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
 * @e: the closure structure with which we were invoked
 * @dev: the #vfi_dev handle currently in use
 * @ah: the #vfi_async_handle currently in use
 * @result: the result string from the driver
 *
 * A sample internal function of the sort which can be invoked by the
 * pipe API pre-command, pipe_pre_cmd(), in conjunction with the
 * source processing loop thread, source_thread(). This function is
 * polymorphic in that it parses its signature to determine its
 * input/output parameters.
 *
 * In reality real functions would at most simply check their
 * signature as a debug reality check before processing their fixed
 * function. 
 * 
 * This function simply prints out the structure of its invoking
 * closure and in reality does nothing.
 *
 * Returns: 0 on error to terminate the pipeline processing loop or 1 to
 * continue pipeline.
 */
extern int show_function(void **e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result);

/**
 * copy_function
 * @e: the closure structure with which we were invoked
 * @dev: the #vfi_dev handle currently in use
 * @ah: the #vfi_async_handle currently in use
 * @result: the result string from the driver
 *
 * Another sample internal function of the sort which can be invoked
 * by the pipe API pre-command,pipe_pre_cmd(), in conjunction with the source
 * processing loop thread, source_thread(). 
 *
 * This function is also polymorphic in that it parses its signature
 * to determine its input/output parameters. However, this function is
 * useful in test scripts as it copies the contents of its input maps
 * to its output maps using only as much data as fits within either
 * the input or output map and resuing the last input map if there are
 * more outputs than inputs.
 *
 * Returns: 0 on error to terminate the pipeline processing loop or 1 to
 * continue pipeline.
 */
extern int copy_function(void **e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result);

/**
 * source_thread
 * @source: a void * which this functions casts to a #vfi_source.
 *
 * This function is invoked via process_commands() for each source
 * which the program utilizes. process_commands() uses the pthread
 * library to process each source in a separate thread.
 *
 * The loop implemented by this program uses vfi_find_pre_cmd() to
 * execute pre-commands, if any, for each command it gets from the
 * @source to execute on the driver using vfi_invoke_cmd() and
 * vfi_wait_async_handle(). 
 *
 * By convention is will skip invocation of the driver if the
 * pre-command returns true, and will execute a closure if returned
 * from vfi_wait_async_handle().
 *
 * Again, by convention, if the closure returns true, the command will
 * be re-invoked against the driver, with the closure once again being
 * invoked when the driver returns.
 *
 * Together these conventions allow loops to be implemented by the API
 * when processing commands from @source scripts. The closure may
 * return true indefinitely or depend upon a count setup in the
 * closure by the pre-command or some other condition from the result
 * from the driver command, or any combination required. The loop may
 * terminate on errors from the driver, loop until the driver command
 * is successful, loop indefintely, or loop subject to some maximum
 * count, etc. All these combinations are possible within this single
 * source_thread() processing conventions by virtue of the closure
 * data and function instigated by the pre-command.
 *
 * Returns: #NULL
 */
extern void *source_thread(void *source);

/**
 * process_commands
 * @tid: output parameter returning thread handle
 * @src: the #vfi_source to be processed by this thread
 * @opts: the gengetops structure to get any options
 *
 * Simple wrapper for the underlying pthreads library used by this
 * application to effect multi-threading behaviour.
 *
 * Returns: the result of the underlying pthreads_create() call.
 */
extern int process_commands(pthread_t *tid, struct vfi_source *src, struct gengetopt_args_info *opts);

/**
 * driver_thread
 * @h: a void * cast to a #vfi_dev handle by this function.
 *
 * This is the function used by the application in a thread to process
 * the driver returns and to effect the return of the results to their
 * respective calling threads using vfi_post_async_handle().
 *
 * Returns: #NULL
 */
extern void *driver_thread(void *h);

/**
 * initialize_api_commands
 * @dev: the #vfi_dev handle currently in use
 *
 * This function simply loads up the #vfi_dev handle @dev with the
 * pre-commands and functions required to process the scripts which
 * this application is designed to handle. Each application would
 * selectively use only those functions and commands which it
 * requires.
 *
 * Commands and functions are loaded with vfi_register_pre_cmd() and
 * vfi_register_func() respectively.
 *
 * Returns: 0
 */
extern int initialize_api_commands(struct vfi_dev *dev);
