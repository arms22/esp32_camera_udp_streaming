import cv2
import numpy as np
import socket
import select
import threading
import queue
import time
import traceback

frame_q = queue.Queue(maxsize=5)
runing = True

def udp_recv(listen_addr, target_addr):

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1)
    sock.bind((listen_addr, 55556))

    # rcvbuf = sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)  # 受信バッファサイズの取得
    # print(rcvbuf)
    # sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 2*16)

    print('Start Streaming...')

    chunks = b''
    while runing:
        try:
            msg, address = sock.recvfrom(2**16)
        except Exception as e:
            # print('sock.recvfrom',e)
            sock.sendto(b'\x55', (target_addr, 55555))
            continue
        soi = msg.find(b'\xff\xd8\xff')
        eoi = msg.rfind(b'\xff\xd9')
        print(time.perf_counter(), len(msg), soi, eoi, msg[:2], msg[-2:])
        if soi >= 0:
            if chunks.startswith(b'\xff\xd8\xff'):
                if eoi >= 0:
                    chunks += msg[:eoi+2]
                    print(time.perf_counter(), "Complete picture")
                    eoi = -1
                else:
                    chunks += msg[:soi]
                    print(time.perf_counter(), "Incomplete picture")
                try:
                    frame_q.put(chunks, timeout=1)
                except Exception as e:
                    print(e)
            chunks = msg[soi:]
        else:
            chunks += msg
        if eoi >= 0:
            eob = len(chunks) - len(msg) + eoi + 2
            if chunks.startswith(b'\xff\xd8\xff'):
                byte_frame = chunks[:eob]
                print(time.perf_counter(), "Complete picture")
                try:
                    frame_q.put(byte_frame, timeout=1)
                except Exception as e:
                    print(e)
            else:
                print(time.perf_counter(), "Invalid picture")
            chunks = chunks[eob:]
    sock.close()
    print('Stop Streaming')

def main(args):
    global runing

    thread = threading.Thread(target=udp_recv, args=(args.listen, args.target))
    thread.start()

    winname = 'frame'
    if args.fullscreen:
        cv2.namedWindow(winname, cv2.WINDOW_NORMAL)
        cv2.setWindowProperty(winname, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)

    while(True):

        try:
            while True:
                byte_frame = frame_q.get(block=True, timeout=1)
                if frame_q.empty():
                    break
                print(time.perf_counter(), 'Skip picture')
            print(time.perf_counter(), 'Decode picture')
            img = cv2.imdecode(np.frombuffer(byte_frame, dtype=np.uint8), 1)

            # # rotate
            # img = cv2.rotate(img, cv2.ROTATE_90_COUNTERCLOCKWISE)

            # # resize
            # width = 800
            # h, w = img.shape[:2]
            # if w < width:
            #     print(time.perf_counter(), 'Resize picture')
            #     height = round(h * (width / w))
            #     img = cv2.resize(img, dsize=(width, height))

            print(time.perf_counter(), 'Show picture')
            cv2.imshow(winname,img)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
        except queue.Empty as e:
            pass
            # print('frame_q.get Empty')
        except Exception as e:
            print(traceback.format_exc())
        except KeyboardInterrupt as e:
            print('KeyboardInterrupt')
            break

    print('Waiting for recv thread to end')
    runing = False
    thread.join()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("listen", type=str)
    parser.add_argument("target", type=str)
    parser.add_argument("--fullscreen", action='store_true')
    args = parser.parse_args()
    main(args)
