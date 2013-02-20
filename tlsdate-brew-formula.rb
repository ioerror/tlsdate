require 'formula'

class Tlsdate < Formula
  homepage 'https://www.github.com/ioerror/tlsdate/'
  url 'https://nodeload.github.com/ioerror/tlsdate/tar.gz/master'
  # This hash will never be correct until we put a tagged version into master
  # update accordingly until we do a proper release that supports OS X
  sha1 '2e818eb327af74c6a6c86a8c8b3911e20be9bc0f'
  version '0.0.6'

  depends_on 'autoconf' => :build
  depends_on 'automake' => :build
  depends_on 'libtool' => :build
  depends_on 'pkg-config' => :build

  def install
    system "./autogen.sh"
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make"
    system "make install"
  end

  def test
    system "tlsdate -v -n"
  end
end
