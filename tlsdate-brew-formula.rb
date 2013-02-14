require 'formula'

class Tlsdate < Formula
  homepage 'https://github.com/ioerror/tlsdate/'
  url 'https://github.com/ioerror/tlsdate/archive/tlsdate-0.0.6.tar.gz'
  sha1 '<update at release time>'

  depends_on 'autoconf' => :build
  depends_on 'automake' => :build
  depends_on 'libtool' => :build
  depends_on 'pkg-config' => :build

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make"
    system "make install"
  end

  def test
    system "tlsdate -v -n"
  end
end
