# Fonts

Put a TrueType font here as `dungeon.ttf`.

This one isn't optional. I let `Renderer::drawText` return early when no font is
loaded, so without this file every label vanishes — and since my menus are
almost entirely text, you get a black window with nothing in it. The panels and
bars still draw underneath, which somehow makes it look more broken, not less.

I hit this myself the first time I ran a clean build and spent a while assuming
the game had crashed.

Any readable pixel font works. "Press Start 2P" and "m5x7" both suit the art.
`C:\Windows\Fonts\consola.ttf` works if you just want to get running.