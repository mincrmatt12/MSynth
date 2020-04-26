# The MSynth UI event system

Goals:

- a lightweight event passing system
- not too many virtual functions or oopiness
- simple to use
- filterable
- supports multiple input sources


## Implementation

In general, there are a few things that the event system has to deal with:
- conditional events (did this touch happen in a button, which handler should get focus for the keyboard)
- dealing with changing receivers 
- avoiding huge amounts of tiny mallocs

Luckily, the system doesn't really need to be super expandable.

