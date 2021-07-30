#ifdef __unix__
#   include <errno.h>
#   include <unistd.h>
#endif

#include "remote-channel.hh"

namespace clap {

   RemoteChannel::RemoteChannel(const MessageHandler &handler,
                                EventControl &evControl,
                                int socket,
                                bool cookieHalf)
      : cookieHalf_(cookieHalf), handler_(handler), evControl_(evControl), socket_(socket) {}

   RemoteChannel::~RemoteChannel() { close(); }

   void RemoteChannel::onRead() {
      ssize_t nbytes = ::read(socket_, inputBuffer_.writeData(), inputBuffer_.writeAvail());
      if (nbytes < 0) {
         if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
            return;

         close();
         return;
      }

      inputBuffer_.wrote(nbytes);
      parseInput();
      inputBuffer_.rewind();
   }

   void RemoteChannel::write(const void *_data, size_t size) {
      const uint8_t *data = static_cast<const uint8_t *>(_data);
      while (size > 0) {
         auto &buffer = nextWriteBuffer();
         buffer.write(data, size);
      }

      assert(size == 0);
   }

   RemoteChannel::WriteBuffer &RemoteChannel::nextWriteBuffer() {
      if (outputBuffers_.empty()) {
         outputBuffers_.emplace();
         return outputBuffers_.back();
      }

      auto &buffer = outputBuffers_.back();
      if (buffer.writeAvail() > 0)
         return buffer;

      outputBuffers_.emplace();
      return outputBuffers_.back();
   }

   void RemoteChannel::onWrite() {
      while (!outputBuffers_.empty()) {
         auto &buffer = outputBuffers_.front();

         auto avail = buffer.readAvail();
         while (avail > 0) {
            auto nbytes = ::write(socket_, buffer.readData(), avail);
            if (nbytes == -1) {
               if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
                  evControl_.modifyFd(CLAP_FD_READ | CLAP_FD_WRITE);
                  return;
               }

               close();
               return;
            }

            buffer.wrote(nbytes);
            avail -= nbytes;
            assert(avail == buffer.readAvail());
         }

         outputBuffers_.pop();
      }

      evControl_.modifyFd(CLAP_FD_READ);
   }

   void RemoteChannel::close() {
      if (socket_ == -1)
         return;

      ::close(socket_);
      socket_ = -1;
   }

   uint32_t RemoteChannel::computeNextCookie() noexcept {
      uint32_t cookie = nextCookie_;
      if (cookieHalf_)
         cookie |= (1ULL << 31);
      else
         cookie &= ~(1ULL << 31);

      ++nextCookie_; // overflow is fine
      return cookie;
   }

   bool RemoteChannel::sendMessageAsync(const Message &msg) {
      write(&msg.type, sizeof(msg.type));
      write(&msg.cookie, sizeof(msg.cookie));
      write(&msg.size, sizeof(msg.size));
      write(msg.data, msg.size);
      onWrite();
      return true;
   }

   bool RemoteChannel::sendMessageSync(const Message &msg, const MessageHandler &handler) {
      return true;
   }
} // namespace clap