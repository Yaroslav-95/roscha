# roscha 

A jinja inspired template engine written in C11. Why would I do something like
this? Because I needed a template engine I could use in C programs, and for the
lulz, of course.

Probably not the best in terms of performance and especially features, but it
gets the job done.

## How to use

All the functions and structures that are needed to use roscha are in
`include/roscha.h`, `include/object.h`, `include/hmap.h`, `include/vector.h`,
and `include/slice.h`.

Basically you initialize roscha with `roscha_init()`, then create a new
environment where all the templates and variables will be with
`roscha_env_new()` add some templates, e.g. you can load them from a dir with
the `roscha_env_load_dir(env, dir)` function, add some variables to `env->vars`
hashmap and render a template running `roscha_env_render(env, template_name)`.

All variables used inside roscha are wrapped around a reference counted
structure called `roscha_object` that also contains the type information needed
by roscha. You should increment and decrement the reference count appropriately
using the functions `roscha_object_ref(object)` and
`roscha_object_unref(object)` accordingly.

After using roscha you should free everything related to roscha by decrementing
the reference counts, destroying the `struct roscha_env *` environment, and
calling `roscha_deinit()`.

## TODO

* Better document this... or not if nobody else uses?
* Probably fix some bugs that are currently hidden.
* k, v arguments in for...in loops over hashmaps
* make hashmaps grow in capacity over a certain load threshold.
* Other stuff like space trimming
