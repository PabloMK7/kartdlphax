# kartdlphax
**kartdlphax** is a semiprimary exploit in the download play mode of Mario Kart 7.

## Installation
The exploit uses a 3GX Plugin in the host system to send modified data to all the client systems. Therefore, in order to use this exploit you need to install the [3GX Loader Luma3DS fork](https://github.com/Nanquitas/Luma3DS/releases/latest).

In the host console, place the `.3gx` file in the following directories:
- EUR: `luma/plugins/0004000000030700`
- JAP: `luma/plugins/0004000000030600`
- USA: `luma/plugins/0004000000030800`

By default, the plugin will use the built-in otherapp payload (universal-otherap). You can place your own otherapp at `/kartdlphax_otherapp.bin`, but keep in mind that homebrew otherapp doesn't work currently.

## Usage
1. On the host 3ds, make sure the plugin loader is enabled from the Rosalina menu (L+Down+Select), then launch the Mario Kart 7 game. (You will see a confirmation message in the top screen).

2. On the client 3ds(es), launch the download play application-

3. On the host 3ds, select `Local Multiplayer` and then `Create Group`. Then let the client 3ds(es) join the group.

4. On the host 3ds, select `Grand Prix` then `50cc` then any driver combination and finally the `Mushroom Cup`. After a while the exploit will trigger on the client 3ds(es).

## Credits
- [universal-otherapp](https://github.com/TuxSH/universal-otherapp) (Copyright (c) 2020 TuxSH)
- [CTRPF](https://gbatemp.net/threads/ctrpluginframework-blank-plugin-now-with-action-replay.487729/) (by Nanquitas)
- [luigoalma](https://github.com/luigoalma) and their exploit [nitpic3d](https://github.com/luigoalma/nitpic3d) for their huge help.
- [Kartic](https://github.com/hax0kartik) for their huge help and all the people from their development discord server.

## Notice
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.