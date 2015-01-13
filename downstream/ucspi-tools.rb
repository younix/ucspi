require "formula"

class UcspiTools < Formula
  homepage "https://github.com/younix/ucspi/blob/master/README.md"
  url "https://github.com/younix/ucspi/tarball/968f1e7231c69624402ae8bf6903880a7491d51a"
  sha1 "643d88d906a51b6c45d3fa52636187932fb68b2b"
  version '2015-01-14'

  depends_on "pkg-config" => :build
  depends_on "ucspi-tcp"

  def install
    system "make", "install prefix=#{prefix}"
  end

  test do
    out = shell_output("#{bin}/socks 2>&1")
    assert out.include?("tcpclient PROXY-HOST PROXY-PORT socks HOST PORT PROGRAM [ARGS...]")
  end
end
