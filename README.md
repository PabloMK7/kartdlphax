# kartdlphax
**kartdlphax** is a semiprimary exploit for the download play mode of [Mario Kart 7](https://en.wikipedia.org/wiki/Mario_Kart_7). It can be used to run an userland payload in an unmodified 3DS by having it connect through download play to another 3DS with Custom Firmware running the exploit.

## Installation
The exploit uses a 3GX Plugin in the host system. Therefore, in order to use this exploit you need to install the [3GX Loader Luma3DS fork](https://github.com/Nanquitas/Luma3DS/releases/latest).

In the host console, place the `.3gx` file from the [Releases page](https://github.com/mariohackandglitch/kartdlphax/releases/latest) in the following directories depending on your game region:
- EUR: `luma/plugins/0004000000030700`
- JAP: `luma/plugins/0004000000030600`
- USA: `luma/plugins/0004000000030800`

(TWN, CHN and KOR regions untested).

By default, the plugin will use the built-in otherapp payload (universal-otherap). You can place your own otherapp at `/kartdlphax_otherapp.bin`, but keep in mind that the hax 2.0 otherapp doesn't work currently.

## Usage
1. On the host 3ds, make sure the plugin loader is enabled from the Rosalina menu (L+Down+Select), then launch the Mario Kart 7 game matching the region of the client 3ds(es). (You will see a confirmation message in the top screen once the game launches).

2. On the client 3ds(es), launch the download play application.

3. On the host 3ds, select `Local Multiplayer` then `Create Group`. After that, let the client 3ds(es) join the group.

4. Once the multiplayer menu loads on the host 3ds, select `Grand Prix` then `50cc` then any driver combination and finally the `Mushroom Cup`. After a while the exploit will trigger on the client 3ds(es).

Keep in mind that while you can send the exploit to 8 consoles at the same time, the success rate seems to decrease for each console added.

## Technical Details
This exploit consists of 3 stages + the otherapp.

1. **Vtable pwn exploit**: The download play child application doesn't have the course files stored in its romfs, so it has to ask the host to send them when needed. Since this data is not part of the child `.cia` and is not signed, we can send anything arbitrary. Furthermore, the client sets up a buffer to recieve the data from the host, but it never checks the incoming data size, so we can produce a buffer overflow which overwrites important data after the recieve buffer. By overwriting a vtable, we can produce an arbitrary jump in the main thread and eventually jump to the ROP chain.
2. **ROP chain**: From the rop chain and using yellows8's 3ds ropkit as a base, we can terminate some problematic threads and replace the area at `0x100000` with the next stage using gspwn. We can't load otherapp directly from ROP because some gadgets and important functions are in the same area as the otherapp target address, so a small helper payload is needed first.
3. **Miniapp payload**: This asm payload based on luigialma's version from nitpic3d is responsable of terminating the rest of the problematic threads, reconstructing the partitioned otherapp from the recieved buffer, mapping it to `0x101000` with gspwn and finally launching it.

You can find more in-depth details in the comments inside the [plugin](plugin/Sources/main.cpp) and [miniapp](3ds_ropkit/miniapp.s) source files.

## Credits
- [3ds ropkit](https://github.com/yellows8/3ds_ropkit) (by [yellows8](https://github.com/yellows8)).
- [universal-otherapp](https://github.com/TuxSH/universal-otherapp) (Copyright (c) 2020 [TuxSH](https://github.com/TuxSH)).
- [CTRPF](https://gbatemp.net/threads/ctrpluginframework-blank-plugin-now-with-action-replay.487729/) (by [Nanquitas](https://github.com/Nanquitas)).
- [nitpic3d](https://github.com/luigoalma/nitpic3d)'s developer [luigoalma](https://github.com/luigoalma) for his huge help.
- [Kartic](https://github.com/hax0kartik) for his huge help and all the people from his development discord server.

## Notice
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.