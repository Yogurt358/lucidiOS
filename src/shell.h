#pragma once

// Forward-declare to avoid pulling limine.h into every includer.
struct limine_framebuffer;

// returns only if something bad happens :(
void shell_run(struct limine_framebuffer* fb);